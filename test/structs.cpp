#include "common.hpp"
#include <string>
int main() {
  // clang-format off
  std::string input {
    "in n : Int\n"
    "struct Foo { x : Int, y : String }\n"
    "def x := Foo { y := \"Hello\", x := n }\n"
    "def k : Foo := Foo { y := x->y, x := n * 2 }\n"
  };
  // clang-format on

  auto script = make_script(input);
}
