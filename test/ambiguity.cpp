#include "common.hpp"
#include <string>

int main() {
  // clang-format off
  std::string input {
    "def foo := pre(foo) ? 1\n"
    "def bar := pre(bar) + 5\n"
    "def baz := pre(baz) + pre(y)\n"
    "def y := 3\n"
    "extern pure a(x : Real) : Real\n"
    "def qux := a(pre(qux))\n"
  };
  // clang-format on

  // Simply compiles
  auto script = make_script(input);
}
