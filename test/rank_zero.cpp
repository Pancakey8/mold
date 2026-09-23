#include "common.hpp"
#include <string>
int main() {
  // clang-format off
  std::string input {
    "def x := pre(x) ? 0 + 1\n"
    "def y := 10\n"
    "def z := not(true)\n"
    "function f(x, y) := x + y\n"
    "def t := f(2, 3)\n"
    "extern pure g(a : Int, b : Int) : Int\n"
    "def k := g(6, -4)\n"
  };
  // clang-format on

  auto script = make_script(input);
  script.implement("g", [](auto args) -> mold::Value {
    auto a = std::get<std::int64_t>(args[0].data);
    auto b = std::get<std::int64_t>(args[1].data);
    return {a + b};
  });

  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  std::println("x = {}", script.read("x"));
  assert(std::get<std::int64_t>(script.read("x").data) == 1);
  std::println("y = {}", script.read("y"));
  assert(std::get<std::int64_t>(script.read("y").data) == 10);
  std::println("z = {}", script.read("z"));
  assert(std::get<bool>(script.read("z").data) == false);
  std::println("t = {}", script.read("t"));
  assert(std::get<std::int64_t>(script.read("t").data) == 5);
  std::println("k = {}", script.read("k"));
  assert(std::get<std::int64_t>(script.read("k").data) == 2);
}
