#include "compiler.hpp"
#include "ast.hpp"
#include "ranking.hpp"
#include "typing.hpp"
#include "utils.hpp"
#include <cstddef>
#include <flat_map>
#include <format>
#include <print>
#include <ranges>
#include <variant>

void Compiler::run() {
  ranks = ranks_of(sorting);

  for (auto [id, r] : ranks) {
    if (r == 0)
      compile(id);
  }
  instrs.emplace_back(HInstr::Term{});

  for (auto [id, r] : ranks) {
    if (r != 0)
      compile(id);
  }

  for (const auto &instr : instrs) {
    std::println("{}", instr.show());
  }
}

void Compiler::compile(NodeId id) {
  std::visit(
      overload{
          [&](const LitInt &n) { instrs.emplace_back(HInstr::Const{n.val}); },
          [&](const LitReal &n) { instrs.emplace_back(HInstr::Const{n.val}); },
          [&](const LitBool &n) { instrs.emplace_back(HInstr::Const{n.val}); },
          [&](const LitString &n) {
            instrs.emplace_back(HInstr::Const{n.val});
          },
          [&](const LitNull &n) {
            instrs.emplace_back(HInstr::Const{std::monostate()});
          },
          [&](const Ident &n) {
            auto it = std::find(locals.rbegin(), locals.rend(), n.name);
            if (it != locals.rend()) {
              std::size_t index = std::distance(locals.begin(), it.base() - 1);
              instrs.emplace_back(HInstr::Comment{
                  std::format("local name = {}", locals[index])});
              instrs.emplace_back(
                  HInstr::Local{static_cast<HInstr::LocId>(index)});
            } else {
              instrs.emplace_back(HInstr::Load{n.name});
            }
          },
          [&](const BinaryOp &n) {
            compile(n.left);
            compile(n.right);
            compile_binop(n.kind);
          },
          [&](const LetIn &n) {
            compile(n.init);
            instrs.emplace_back(
                HInstr::Comment{std::format("push local {}", n.name)});
            instrs.emplace_back(HInstr::PushLocal{});
            locals.push_back(n.name);
            compile(n.body);
            instrs.emplace_back(
                HInstr::Comment{std::format("pop local {}", n.name)});
            instrs.emplace_back(HInstr::PopLocal{});
            locals.pop_back();
          },
          [&](const IfElse &n) {
            compile(n.cond);
            auto join = fresh_label();
            auto fals = fresh_label();
            instrs.emplace_back(HInstr::JumpFalse{fals});
            compile(n.tru);
            instrs.emplace_back(HInstr::Jump{join});
            instrs.emplace_back(HInstr::Label{fals});
            if (n.fals != NODEID_NONE) {
              compile(n.fals);
            } else {
              instrs.emplace_back(HInstr::Const{std::monostate()});
            }
            instrs.emplace_back(HInstr::Label{join});
          },
          [&](const PreVal &n) {
            instrs.emplace_back(
                HInstr::LoadAt{n.name, static_cast<uint16_t>(n.depth)});
          },
          [&](const FuncCall &n) {
            instrs.emplace_back(
                HInstr::Comment{std::format("params {}:", n.name)});
            std::size_t i = 0;
            for (auto p : n.params) {
              instrs.emplace_back(
                  HInstr::Comment{std::format("- param #{}:", ++i)});
              compile(p);
            }
            if (events.contains(n.name)) {
              instrs.emplace_back(HInstr::Ctor{
                  n.name, static_cast<std::uint16_t>(n.params.size())});
            } else {
              instrs.emplace_back(HInstr::Call{
                  n.name, static_cast<std::uint16_t>(n.params.size())});
            }
          },
          [&](const TypeName &n) {}, // N/A here
          [&](const Input &n) {
            instrs.emplace_back(HInstr::Comment{
                std::format("in {} : {}", n.name, ast[n.type].type.show())});
            instrs.emplace_back(HInstr::Pull{n.name});
            auto l = fresh_label();
            instrs.emplace_back(HInstr::JumpFalse{l});
            if (auto it = sorting.deps.find(id); it != sorting.deps.end()) {
              for (auto dep : it->second) {
                instrs.emplace_back(
                    HInstr::Mp{ast[dep].toplevel_name(),
                               static_cast<uint16_t>(ranks.at(dep))});
              }
            }
            instrs.emplace_back(HInstr::Label{l});
          },
          [&](const Output &n) { events.insert(n.name); },
          [&](const Formula &n) {
            instrs.emplace_back(HInstr::VertLabel{n.name});
            compile(n.init);
            instrs.emplace_back(HInstr::Store{n.name});
            instrs.emplace_back(HInstr::Pull{n.name});
            auto l = fresh_label();
            instrs.emplace_back(HInstr::JumpFalse{l});
            if (auto it = sorting.deps.find(id); it != sorting.deps.end()) {
              for (auto dep : it->second) {
                instrs.emplace_back(
                    HInstr::Mp{ast[dep].toplevel_name(),
                               static_cast<std::uint16_t>(ranks.at(dep))});
              }
            }
            instrs.emplace_back(HInstr::Label{l});
            instrs.emplace_back(HInstr::Term{});
          },
          [&](const Signal &n) {
            instrs.emplace_back(HInstr::VertLabel{n.name});
            compile(n.init);
            instrs.emplace_back(HInstr::Store{n.name});
            instrs.emplace_back(HInstr::Pull{n.name});
            auto l = fresh_label();
            instrs.emplace_back(HInstr::JumpFalse{l});
            if (auto it = sorting.deps.find(id); it != sorting.deps.end()) {
              for (auto dep : it->second) {
                instrs.emplace_back(
                    HInstr::Mp{ast[dep].toplevel_name(),
                               static_cast<std::uint16_t>(ranks.at(dep))});
              }
            }
            instrs.emplace_back(HInstr::Dispatch{n.name});
            instrs.emplace_back(HInstr::Label{l});
            instrs.emplace_back(HInstr::Term{});
          },
          [&](const Extern &n) {
            if (!n.pure) {
              if (auto it = sorting.deps.find(id); it != sorting.deps.end()) {
                for (auto dep : it->second) {
                  instrs.emplace_back(
                      HInstr::Mp{ast[dep].toplevel_name(),
                                 static_cast<std::uint16_t>(ranks.at(dep))});
                }
              }
            }
          },
          [&](const Function &n) {}, // N/A here
          [&](const Error &n) { assert(false && "TODO: Unreachable?"); },
          [&](const Inline &n) {
            for (auto par : n.params | std::ranges::views::reverse) {
              compile(par);
            }
            for (auto name : n.names) {
              instrs.emplace_back(
                  HInstr::Comment{std::format("push local {}", name)});
              instrs.emplace_back(HInstr::PushLocal{});
              locals.push_back(name);
            }
            compile(n.formula);
            for (auto name : n.names) {
              instrs.emplace_back(
                  HInstr::Comment{std::format("pop local {}", name)});
              instrs.emplace_back(HInstr::PopLocal{});
              locals.pop_back();
            }
          },
      },
      ast[id].data);
}

void Compiler::compile_binop(BinaryOp::Kind id) {
  switch (id) {
  case BinaryOp::AND:
    instrs.emplace_back(HInstr::And{});
    break;
  case BinaryOp::OR:
    instrs.emplace_back(HInstr::Or{});
    break;
  case BinaryOp::BITAND:
    instrs.emplace_back(HInstr::BitAnd{});
    break;
  case BinaryOp::BITOR:
    instrs.emplace_back(HInstr::BitOr{});
    break;
  case BinaryOp::EQ:
    instrs.emplace_back(HInstr::Eq{});
    break;
  case BinaryOp::NEQ:
    instrs.emplace_back(HInstr::Neq{});
    break;
  case BinaryOp::LT:
    instrs.emplace_back(HInstr::Lt{});
    break;
  case BinaryOp::GT:
    instrs.emplace_back(HInstr::Gt{});
    break;
  case BinaryOp::LE:
    instrs.emplace_back(HInstr::Le{});
    break;
  case BinaryOp::GE:
    instrs.emplace_back(HInstr::Ge{});
    break;
  case BinaryOp::SHL:
    instrs.emplace_back(HInstr::Shl{});
    break;
  case BinaryOp::SHR:
    instrs.emplace_back(HInstr::Shr{});
    break;
  case BinaryOp::ADD:
    instrs.emplace_back(HInstr::Add{});
    break;
  case BinaryOp::SUB:
    instrs.emplace_back(HInstr::Sub{});
    break;
  case BinaryOp::MULT:
    instrs.emplace_back(HInstr::Mult{});
    break;
  case BinaryOp::DIV:
    instrs.emplace_back(HInstr::Div{});
    break;
  case BinaryOp::MOD:
    instrs.emplace_back(HInstr::Mod{});
    break;
  case BinaryOp::COAL:
    instrs.emplace_back(HInstr::Coal{});
    break;
  }
}

std::uint32_t Compiler::fresh_label() { return labels++; }

std::string const_show(const HInstr::Const &c) {
  return std::visit(
      overload{[](std::monostate) -> std::string { return "null"; },
               [](auto &&v) -> std::string { return std::format("{}", v); }},
      c.val);
}

std::string HInstr::show() const {
  if (auto com = std::get_if<Comment>(&data)) {
    return std::format("# {}", com->message);
  }

  std::string_view tag;
#define X(N, I)                                                                \
  I(std::holds_alternative<N>(data)) { tag = #N; }
#define F(N) X(N, if)
#define R(N) X(N, else if)
  HINSTR_KIND_LIST(F, R)
#undef X
#undef F
#undef R

  // clang-format off
  auto d = std::visit(overload{
    [](const Const &n) -> std::string { return const_show(n); },
    [](const Load& n) -> std::string { return std::format("[{}]", n.glob); },
    [](const Store& n) -> std::string { return std::format("[{}]", n.glob); },
    [](const Local& n) -> std::string { return std::format("[local_{}]", n.loc); },
    [](const JumpFalse& n) -> std::string { return std::format("L{}", n.label); },
    [](const JumpTrue& n) -> std::string { return std::format("L{}", n.label); },
    [](const Jump& n) -> std::string { return std::format("L{}", n.label); },
    [](const Label& n) -> std::string { return std::format("L{}", n.label); },
    [](const LoadAt& n) -> std::string { return std::format("[{}], {}", n.glob, n.depth); },
    [](const Call& n) -> std::string { return std::format("{}({})", n.fn, n.argc); },
    [](const Ctor& n) -> std::string { return std::format("{}({})", n.fn, n.argc); },
    [](const Mp& n) -> std::string { return std::format("[{}], {}", n.vert, n.rank); },
    [](const VertLabel& n) -> std::string { return std::format("[{}]", n.vert); },
    [](const Pull& n) -> std::string { return std::format("[{}]", n.glob); },
    [](const Dispatch& n) -> std::string { return std::format("[{}]", n.glob); },
    [](auto&&) -> std::string { return ""; }
  }, data);
  // clang-format on

  return std::format("{} {}", tag, d);
}
