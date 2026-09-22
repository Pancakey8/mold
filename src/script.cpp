#include "mold/script.hpp"
#include "mold/compiler.hpp"
#include "mold/interpreter.hpp"
#include "mold/parser.hpp"
#include "mold/sort.hpp"
#include "mold/typing.hpp"
#include "mold/utils.hpp"
#include <cstdint>
#include <print>
#include <unordered_map>
#include <variant>

namespace mold {
using namespace internal;

struct TransparentStringHash {
  using is_transparent = void;

  std::size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
  std::size_t operator()(const std::string &s) const noexcept {
    return std::hash<std::string>{}(s);
  }
  std::size_t operator()(const char *s) const noexcept {
    return std::hash<std::string_view>{}(s);
  }
};

template <typename T>
using StringMap = std::unordered_map<std::string, T, TransparentStringHash,
                                     std::equal_to<void>>;

struct Script::Impl {
  Program prog;
  StringMap<std::uint32_t> vars, inputs, events, externs, structs;
  std::vector<std::string> event_names;
  std::vector<std::string> struct_names;
  std::vector<std::vector<std::string>> struct_fields;
  Interpreter interp;

  explicit Impl(Program p) : prog(std::move(p)), interp(prog) {}

  InternValue of_public(const Value &v) const;
  Value of_intern(const InternValue &ival) const;
};

Script::~Script() = default;
Script::Script(Script &&) noexcept = default;
Script &Script::operator=(Script &&) noexcept = default;

Script::Script(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}

std::expected<Script, std::vector<mold::Diagnostic>>
Script::of_string(std::string_view input) {
  auto ast = Parser{{input}}.parse();
  // for (auto id = ast.begin(); id != ast.end(); ++id) {
  //   std::println("{}", ast[id].show(ast.pool));
  // }
  auto [sorting, sort_diags] = TopoSort{ast}.run();
  auto [typed, ast_diags] = Typing{ast, sorting}.run();
  if (!sort_diags.empty() || !ast_diags.empty()) {
    std::vector<mold::Diagnostic> diags{};
    diags.reserve(sort_diags.size() + ast_diags.size());

    for (auto &diag : sort_diags) {
      diags.push_back({diag.message, {diag.src.start, diag.src.end}});
    }

    for (auto &diag : ast_diags) {
      diags.push_back({diag.message, {diag.src.start, diag.src.end}});
    }

    return std::unexpected(std::move(diags));
  }
  // for (auto id = typed.begin(); id != typed.end(); ++id) {
  //   std::println("TYPED {}", typed[id].show(typed.pool));
  // }
  auto hir = Compiler{typed}.run();
  // for (const auto &instr : hir) {
  //   std::println("{}", instr.show());
  // }
  auto [prog, syms] = HighToLow{hir}.run();
  // for (const auto &instr : prog.instrs) {
  //   std::println("{}", instr.show());
  // }
  StringMap<std::uint32_t> vars{};
  for (std::size_t id = 0; id < syms.globs.size(); ++id) {
    vars[std::string{syms.globs[id]}] = static_cast<std::uint32_t>(id);
  }
  StringMap<std::uint32_t> inputs{};
  for (const auto &tl : typed.tls) {
    if (tl != NODEID_NONE && std::holds_alternative<Input>(typed[tl].data)) {
      auto name = typed[tl].toplevel_name();
      if (auto it = vars.find(name); it != vars.end()) {
        inputs.insert(*it);
      }
    }
  }
  StringMap<std::uint32_t> events{};
  std::vector<std::string> event_names{};
  for (std::size_t id = 0; id < syms.events.size(); ++id) {
    events[std::string{syms.events[id]}] = static_cast<std::uint32_t>(id);
    event_names.push_back(std::string{syms.events[id]});
  }
  StringMap<std::uint32_t> externs{};
  for (std::size_t id = 0; id < syms.exts.size(); ++id) {
    externs[std::string{syms.exts[id]}] = static_cast<std::uint32_t>(id);
  }
  StringMap<std::uint32_t> structs{};
  std::vector<std::string> struct_names{};
  std::vector<std::vector<std::string>> struct_fields{};
  for (std::size_t id = 0; id < syms.structs.size(); ++id) {
    structs[std::string{syms.structs[id]}] = static_cast<std::uint32_t>(id);
    struct_names.push_back(std::string{syms.structs[id]});
    std::vector<std::string> fields{};
    for (auto field : syms.fields[syms.structs[id]])
      fields.push_back(std::string{field});
    struct_fields.push_back(std::move(fields));
  }
  // std::println("{}\n{}", vars, inputs);
  auto impl = std::make_unique<Impl>(std::move(prog));
  impl->vars = std::move(vars);
  impl->inputs = std::move(inputs);
  impl->events = std::move(events);
  impl->externs = std::move(externs);
  impl->event_names = std::move(event_names);
  impl->structs = std::move(structs);
  impl->struct_names = std::move(struct_names);
  impl->struct_fields = std::move(struct_fields);

  return Script{std::move(impl)};
}

std::expected<void, std::string_view> Script::tick() {
  impl->interp.tick();
  if (impl->interp.has_error) {
    impl->interp.has_error = false;
    return std::unexpected(impl->interp.error);
  }
  return {};
}

InternValue Script::Impl::of_public(const Value &v) const {
  return std::visit(
      overload{
          [](std::int64_t n) -> InternValue {
            return {{.i = n}, InternValue::INT};
          },
          [](double n) -> InternValue { return {{.r = n}, InternValue::REAL}; },
          [](bool n) -> InternValue { return {{.b = n}, InternValue::BOOL}; },
          [](const std::string &n) -> InternValue {
            return InternValue::of_string(n);
          },
          [](std::monostate) -> InternValue { return {{}, InternValue::NIL}; },
          [this](Event e) -> InternValue {
            std::vector<InternValue> args{};
            args.reserve(e.args.size());
            for (const auto &arg : e.args)
              args.push_back(of_public(arg));
            if (auto it = events.find(e.kind); it != events.end()) {
              return InternValue::of_event(it->second, args);
            } else {
              return InternValue::of_event(-1, args);
            }
          },
          [](Date d) -> InternValue {
            return {{.d = d.time_since_epoch().count()}, InternValue::DATE};
          },
          [](Time t) -> InternValue {
            return {{.t = t.count()}, InternValue::TIME};
          },
          [this](Struct s) -> InternValue {
            if (auto it = structs.find(s.kind); it != structs.end()) {
              std::vector<InternValue> fields{};
              fields.reserve(s.fields.size());
              for (const auto &k : struct_fields[it->second]) {
                fields.push_back(of_public(s.fields[k]));
              }
              return InternValue::of_struct(it->second, fields);
            } else {
              return InternValue::of_struct(-1, {});
            }
          }},
      v.data);
}

Value Script::Impl::of_intern(const InternValue &ival) const {
  switch (ival.tag) {
  case internal::InternValue::NIL:
    return {std::monostate()};
  case internal::InternValue::INT:
    return {ival.data.i};
  case internal::InternValue::REAL:
    return {ival.data.r};
  case internal::InternValue::BOOL:
    return {ival.data.b};
  case internal::InternValue::STRING:
    return {std::string{ival.as_string()}};
  case internal::InternValue::EVENT: {
    std::vector<Value> vs{};
    vs.reserve(ival.data.e->argc);
    for (std::uint16_t i = 0; i < ival.data.e->argc; ++i) {
      vs.push_back(of_intern(ival.data.e->args[i]));
    }
    auto &name = event_names[ival.data.e->tag];
    return {Event{name, std::move(vs)}};
  } break;
  case internal::InternValue::DATE:
    return {Date{std::chrono::nanoseconds{ival.data.d}}};
  case internal::InternValue::TIME:
    return {Time{ival.data.t}};
  case internal::InternValue::STRUCT:
    auto &name = struct_names[ival.data.st->tag];
    auto &field_names = struct_fields[ival.data.st->tag];
    std::unordered_map<std::string_view, Value> fields{};
    fields.reserve(ival.data.st->fieldc);
    for (std::uint16_t i = 0; i < ival.data.st->fieldc; ++i) {
      fields[field_names[i]] = of_intern(ival.data.st->fields[i]);
    }
    return {Struct{name, std::move(fields)}};
  }
}

void Script::feed(std::string_view name, Value v) {
  auto ival = impl->of_public(v);
  if (auto it = impl->inputs.find(name); it != impl->inputs.end()) {
    impl->interp.feed(it->second, ival);
  }
}

Value Script::read(std::string_view name) {
  auto it = impl->vars.find(name);
  if (it == impl->vars.end()) {
    return {std::monostate()};
  }
  auto ival = impl->interp.read(it->second);
  Value v = impl->of_intern(ival);
  ival.dec();
  return v;
}

void Script::implement(std::string_view name, ExtFn fn) {
  auto it = impl->externs.find(name);
  if (it == impl->externs.end()) {
    return;
  }
  impl->interp.implement(
      it->second,
      [impl = impl.get(), fn = std::move(fn)](
          std::uint16_t argc, InternValue *argv) -> InternValue {
        std::vector<Value> args{};
        args.reserve(argc);
        for (std::uint16_t i = 0; i < argc; ++i) {
          args.push_back(impl->of_intern(argv[i]));
        }
        auto res = fn(args);
        return impl->of_public(res);
      });
}

void Script::on(std::string_view name, HandlerFn fn) {
  auto it = impl->events.find(name);
  if (it == impl->events.end()) {
    return;
  }
  impl->interp.on(it->second, [impl = impl.get(), fn = std::move(fn)](
                                  std::uint16_t argc, InternValue *argv) {
    std::vector<Value> args{};
    args.reserve(argc);
    for (std::uint16_t i = 0; i < argc; ++i) {
      args.push_back(impl->of_intern(argv[i]));
    }
    fn(args);
  });
}

std::string mold::Diagnostic::format(std::string_view input) {
  std::size_t line = 1;
  std::size_t col = 1;

  for (std::size_t i = 0; i < src.start && i < input.size(); ++i) {
    if (input[i] == '\n') {
      line++;
      col = 1;
    } else {
      col++;
    }
  }

  auto start = std::clamp(src.start, 0UZ, input.size() - 1);
  auto end = std::clamp(src.end, 0UZ, input.size() - 1);
  std::string_view source_str =
      std::string_view(input).substr(start, end - start);

  return std::format("{}:{}: {}\nAt {}", line, col, message, source_str);
}
}; // namespace mold
