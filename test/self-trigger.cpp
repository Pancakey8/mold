#include <cstdint>
#include <ranges>
#include <string>
#include "common.hpp"

int main() {
  // clang-format off
  std::string input1 {
    "def x := max(1, pre(x) + 1)"
    "def y := x * 2"
  };
  // clang-format on

  auto script1 = make_script(input1);

  for (std::int64_t t = 1; t < 10; ++t) {
    if (auto res = script1.tick(); !res.has_value()) {
      std::println("{}", res.error());
      assert(false && "Interpreter crash");
    }
    std::println("x = {} | y = {}", script1.read("x"), script1.read("y"));
    assert(std::get<std::int64_t>(script1.read("x").data) == t);
    assert(std::get<std::int64_t>(script1.read("y").data) == t * 2);
  }

  // clang-format off
  std::string input2 {
    "in a : Int\n"
    "in b : Int\n"
    "def x := y + a\n"
    "def y := x + b\n"
  };
  // clang-format on

  assert(fail_compile(input2));

  // clang-format off
  std::string input3 {
    "in a : Int\n"
    "in b : Int\n"
    "def x := pre(y) ? 0 + a\n"
    "def y := pre(x) ? 0 + b\n"    
  };
  // clang-format on

  auto script3 = make_script(input3);

  std::vector<std::int64_t> as{5, 9, 1, 1, 6};
  std::vector<std::int64_t> bs{2, 2, 3, 7, -4};
  std::vector<std::int64_t> xs{5, 11, 3, 3, 16};
  std::vector<std::int64_t> ys{2, 2, 14, 10, -1};

  for (auto [a, b, x, y] : std::views::zip(as, bs, xs, ys)) {
    script3.feed("a", {a});
    script3.feed("b", {b});
    if (auto res = script3.tick(); !res.has_value()) {
      std::println("{}", res.error());
      assert(false && "Interpreter crash");
    }
    std::println("x = {} | y = {}", script3.read("x"), script3.read("y"));
    assert(std::get<std::int64_t>(script3.read("x").data) == x);
    assert(std::get<std::int64_t>(script3.read("y").data) == y);
  }

  // clang-format off
  std::string input4 {
    "def x := not(pre(x)) ? true\n"
    "extern pure my_not(b : Bool) : Bool\n"
    "def y := not(pre(x) ? false)\n"
  };
  // clang-format on

  auto script4 = make_script(input4);
  script4.implement("my_not", [](auto args) -> mold::Value {
    auto b = std::get<bool>(args[0].data);
    return {!b};
  });

  for (std::int64_t t = 1; t < 10; ++t) {
    if (auto res = script4.tick(); !res.has_value()) {
      std::println("{}", res.error());
      assert(false && "Interpreter crash");
    }
    std::println("x = {} | y = {}", script4.read("x"), script4.read("y"));
    assert(std::get<bool>(script4.read("x").data) == (t % 2));
    assert(std::get<bool>(script4.read("y").data) == (t % 2));
  }
}
