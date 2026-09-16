#include "common.hpp"
#include "mold/script.hpp"
#include <string>

int main() {
  // clang-format off
  std::string input {
    "function f(x, y) := x + g(y)\n"
    "function g(n) := let n := n + 1 in h(n)\n"
    "extern pure h(n : Real) : Real\n"
    "def foo := f(x, 2 * x)\n"
    "in x : Int\n"
  };
  // clang-format on

  auto script = make_script(input);
  script.implement("h", [](auto args) -> mold::Value {
    auto n = std::get<double>(args[0].data);
    return {n * n + 1.0};
  });

  script.feed("x", {5});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  // 5 + ((10 + 1) * (10 + 1) + 1) == 5 + 121 + 1 == 127
  assert(std::get<double>(script.read("foo").data) == 127.0);
}
