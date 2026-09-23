#include "common.hpp"
#include <string>
#include <variant>

int main() {
  // clang-format off
  std::string input {
    "extern impure assert(b : Bool?) : Bool\n"
    "def inull : Int? := null\n"

    "def x1 := assert(null < 3)\n"
    "def x2 := assert(not(3 < null))\n"
    "def x3 := assert(not(inull < inull))\n"

    "def x4 := assert(not(null > 3))\n"
    "def x5 := assert(3 > null)\n"
    "def x6 := assert(not(inull > inull))\n"

    "def x7 := assert(null <= 3)\n"
    "def x8 := assert(not(3 <= null))\n"
    "def x9 := assert(inull <= inull)\n"

    "def x10 := assert(not(null >= 3))\n"
    "def x11 := assert(3 >= null)\n"
    "def x12 := assert(inull >= inull)\n"
  };
  // clang-format on

  auto script = make_script(input);
  std::size_t pass{0};
  script.implement("assert", [&pass](auto args) -> mold::Value {
    bool b{false};
    if (!std::holds_alternative<std::monostate>(args[0].data)) {
      b = std::get<bool>(args[0].data);
    }
    assert(b && "Assertion failed");
    ++pass;
    return {b};
  });

  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  std::println("{}", pass);

  assert(pass == 12);
}
