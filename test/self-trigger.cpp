#include <cstdint>
#include <ranges>
#include <string>
#include "common.hpp"

int main() {
    // clang-format off
  std::string input {
    "def x := max(1, pre(x) + 1)"
  };
  // clang-format on

  auto script = make_script(input);
  script.on("tick", [](auto args) {
    std::println("On tick {}", args[0]);
  });

  for (std::int64_t t = 1; t < 10; ++t) {
    if (auto res = script.tick(); !res.has_value()) {
      std::println("{}", res.error());
      assert(false && "Interpreter crash");
    }
    assert(std::get<std::int64_t>(script.read("x").data) == t);
  }
}
