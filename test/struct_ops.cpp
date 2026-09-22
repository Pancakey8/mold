#include "common.hpp"
#include <string>

int main() {
  // clang-format off
  std::string input1 {
    "struct Foo { x : Int }\n"
    "struct Bar { x : Int }\n"
    "def x := Foo { x := 3 }\n"
    "def y := Bar { x := 5 }\n"
    "def z := x = y\n"
  };
  // clang-format on

  assert(fail_compile(input1));

  // clang-format off
  std::string input2 {
    "in k : Int\n"
    "struct Foo { x : Int }\n"
    "def x := Foo { x := k + 2 }\n"
    "def y := Foo { x := k - 1 }\n"
    "def z := Foo { x := k * 2 }\n"
    "def a := x = y\n"
    "def b := x = z\n"
  };
  // clang-format on

  auto script = make_script(input2);

  script.feed("k", {2});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    return 1;
  }

  std::println("x = {}", script.read("x"));
  std::println("y = {}", script.read("y"));
  std::println("z = {}", script.read("z"));

  assert(std::get<bool>(script.read("a").data) == false);
  assert(std::get<bool>(script.read("b").data) == true);
}
