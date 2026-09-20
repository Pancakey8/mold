#include "common.hpp"
#include "mold/script.hpp"
#include <cassert>
#include <cstdint>
#include <print>

int main() {
  // clang-format off
  std::string input {
    "def fib := pre(fib) ? 1 + pre(fib, 2) ? 0\n"
  };
  // clang-format on

  auto script = make_script(input);

  std::int64_t results[]{
      1, 1, 2, 3, 5, 8, 13, 21, 34, 55,
  };

  for (std::size_t i = 0; i < 10; ++i) {
    if (auto res = script.tick(); !res.has_value()) {
      std::println("{}", res.error());
      assert(false && "Interpreter crashed");
      break;
    }
    assert(results[i] == std::get<std::int64_t>(script.read("fib").data));
  }
}
