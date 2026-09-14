#include "script.hpp"
#include "compiler.hpp"
#include "interpreter.hpp"
#include "parser.hpp"
#include "sort.hpp"
#include "typing.hpp"
#include "unordered_map"
#include "utils.hpp"
#include <cstdint>
#include <print>
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
  StringMap<std::uint32_t> vars, inputs;
  Interpreter interp;

  Impl(Program p, StringMap<std::uint32_t> vars, StringMap<std::uint32_t> ins)
      : prog(std::move(p)), vars(std::move(vars)), inputs(std::move(ins)),
        interp(prog) {}
};

Script::~Script() = default;

Script::Script(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}

Script Script::of_string(std::string_view input) {
  auto ast = Parser{{input}}.parse();
  auto sorting = topo_sort(ast);
  auto typed = Typing{ast, sorting}.run();
  auto typed_sort = migrate_sort(typed, sorting);
  auto hir = Compiler{typed, typed_sort}.run();
  auto [prog, syms] = HighToLow{hir}.run();
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
  // std::println("{}\n{}", vars, inputs);
  auto impl = std::make_unique<Impl>(std::move(prog), std::move(vars),
                                     std::move(inputs));
  return Script{std::move(impl)};
}

void Script::tick() { impl->interp.tick(); }

void Script::feed(std::string_view name, Value v) {
  auto ival = std::visit(
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
      },
      v.data);
  if (auto it = impl->inputs.find(name); it != impl->inputs.end()) {
    impl->interp.feed(it->second, ival);
  } else {
    // TODO: Error handling
  }
}

Value Script::read(std::string_view name) {
  auto it = impl->vars.find(name);
  if (it == impl->vars.end()) {
    // TODO: Error handling
    return {std::monostate()};
  }
  auto ival = impl->interp.read(it->second);
  Value v;
  switch (ival.tag) {
  case internal::InternValue::NIL:
    v = {std::monostate()};
    break;
  case internal::InternValue::INT:
    v = {ival.data.i};
    break;
  case internal::InternValue::REAL:
    v = {ival.data.r};
    break;
  case internal::InternValue::BOOL:
    v = {ival.data.b};
    break;
  case internal::InternValue::STRING:
    v = {std::string{ival.as_string()}};
    break;
  }
  ival.dec();
  return v;
}

}; // namespace mold
