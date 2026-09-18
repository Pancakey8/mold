#include "common.hpp"
#include "mold/script.hpp"
#include <string>

int main() {
  // clang-format off
  std::string input {
    "extern pure Int.f(a : Int) : Int\n"
    "extern pure String.f(a : String) : String\n"
    "extern impure Int.acc(n : Int) : Int\n"
    "function Int.g(a, b) := a + b\n"
    "function Real.g(a, b) := a - b\n"
    "in x : Int\n"
    "in y : String\n"
    "def foo := f(x)\n"
    "def bar := f(y)\n"
    "def baz := g(3, x)\n"
    "def qux := g(3.0, x)\n"
    "def always := acc(x)\n"
    "def builtin := max(x, 3)\n"
    "def annot : Int? := g(pre(annot), 1)\n"
  };
  // clang-format on

  auto script = make_script(input);
  script.implement("Int.f", [](auto args) -> mold::Value {
    return {std::get<std::int64_t>(args[0].data) + 1};
  });
  script.implement("String.f", [](auto args) -> mold::Value {
    return {std::get<std::string>(args[0].data) + "foo"};
  });
  std::int64_t acc{0};
  script.implement("Int.acc", [&acc](auto args) -> mold::Value {
    acc += std::get<std::int64_t>(args[0].data);
    return {acc};
  });

  script.feed("x", {10});
  script.feed("y", {"Hello"});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  assert(std::get<std::int64_t>(script.read("foo").data) == 11);
  assert(std::get<std::string>(script.read("bar").data) == "Hellofoo");
  assert(std::get<std::int64_t>(script.read("baz").data) == 13);
  assert(std::get<double>(script.read("qux").data) == -7.0);
  assert(std::get<std::int64_t>(script.read("always").data) == 20);
  assert(std::get<std::int64_t>(script.read("builtin").data) == 10);
  assert(std::holds_alternative<std::monostate>(script.read("annot").data));
}
