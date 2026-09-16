#include "mold/script.hpp"
#include <cassert>
#include <cstdint>
#include <print>

int main() {
  auto res = mold::Script::of_string("extern impure every() : Bool\ndef fib := if every() then pre(fib) ? 1 + pre(fib, 2) ? 0");
  if (!res.has_value()) assert(false && "Compile error");

  auto &script = *res;
  script.implement("every", [](auto) -> mold::Value { return {true}; });

  std::int64_t results[] {
    1,
    1,
    2,
    3,
    5,
    8,
    13,
    21,
    34,
    55,
  };

  for (std::size_t i = 0; i < 10; ++i) {
    if (auto res = script.tick(); !res.has_value()) {
      std::println("{}", res.error());
      assert(false);
      break;
    }
    assert(results[i] == std::get<std::int64_t>(script.read("fib").data));
  }
}
