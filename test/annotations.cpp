#include "common.hpp"
#include <string>
int main() {
  // clang-format off
  std::string input {
    "def bar : Real := pre(foo) ? 1.0\n"
    "def foo : Real? := pre(bar) + 1.0\n"
    "def baz : Int? := max(pre(baz), x)\n"
    "in x : Int\n"
  };
  // clang-format on

  // Simply compiles
  auto script = make_script(input);
}
