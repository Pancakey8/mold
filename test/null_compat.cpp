#include "common.hpp"
#include <cassert>
#include <string>

int main() {
  // clang-format off
  std::string input1 {
    "extern pure f(x : Int?) : Int\n"
    "def foo := f(null)\n"
    "def bar := f(pre(y))\n"
    "def y := 3\n"
  };
  // clang-format on

  // Simply compiles
  auto script1 = make_script(input1);

  // clang-format off
  std::string input2 {
    "extern pure f(x : Int) : Int\n"
    "def foo := f(null)\n"
  };
  // clang-format on

  assert(fail_compile(input2));

  // clang-format off
  std::string input3 {
    "extern pure f(x : Int) : Int\n"
    "def bar := f(pre(y))\n"
    "def y := 3\n"
  };
  // clang-format on

  assert(fail_compile(input3));

  // clang-format off
  std::string input4 {
    "in x : Int?\n"
    "in b : Bool?\n"
    "in y : Int\n"
    "def eq := x = y\n"
    "def comp := x < y\n"
    "def amp := b && eq\n"
    "def norm := x + y\n"
  };
  // clang-format on

  auto script4 = make_script(input4);

  script4.feed("x", {});
  script4.feed("b", {});
  script4.feed("y", {3});
  if (auto res = script4.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  std::println("eq = {} | comp = {} | amp = {} | norm = {}", script4.read("eq"),
               script4.read("comp"), script4.read("amp"), script4.read("norm"));

  assert(std::get<bool>(script4.read("eq").data) == false);
  assert(std::get<bool>(script4.read("comp").data) == true);
  assert(std::holds_alternative<std::monostate>(script4.read("amp").data));
  assert(std::holds_alternative<std::monostate>(script4.read("norm").data));

  script4.feed("x", {5});
  script4.feed("b", {true});
  script4.feed("y", {3});
  if (auto res = script4.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  assert(std::get<bool>(script4.read("eq").data) == false);
  assert(std::get<bool>(script4.read("comp").data) == false);
  assert(std::get<bool>(script4.read("amp").data) == false);
  assert(std::get<std::int64_t>(script4.read("norm").data) == 8);

  std::println("eq = {} | comp = {} | amp = {} | norm = {}", script4.read("eq"),
               script4.read("comp"), script4.read("amp"), script4.read("norm"));
}
