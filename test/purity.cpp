#include "common.hpp"
#include "mold/builtins.hpp"
#include "mold/interpreter.hpp"
#include "mold/script.hpp"
#include "mold/typing.hpp"
#include <cstddef>
#include <cstdint>
#include <print>
#include <string>

int main() {
  std::int64_t acc_val{0};
  auto builtin_acc = [&acc_val](std::int16_t, mold::internal::InternValue *argv)
      -> mold::internal::InternValue {
    acc_val += argv[0].data.i;
    std::println("acc = {}", acc_val);
    return {{.i = acc_val}, mold::internal::InternValue::INT};
  };
  mold::internal::add_builtin({"acc",
                               {{mold::internal::MoldType::INT, false}},
                               {mold::internal::MoldType::INT, false},
                               false,
                               builtin_acc});
  std::int64_t acc_valp{0};
  auto builtin_accp = [&acc_valp](std::int16_t, mold::internal::InternValue *argv)
      -> mold::internal::InternValue {
    acc_valp += argv[0].data.i;
    std::println("accp = {}", acc_valp);
    return {{.i = acc_valp}, mold::internal::InternValue::INT};
  };
  mold::internal::add_builtin({"accp",
                               {{mold::internal::MoldType::INT, false}},
                               {mold::internal::MoldType::INT, false},
                               true,
                               builtin_accp});

  // clang-format off
  std::string input {
    "in y : Int\n"
    "extern impure f(n : Int) : Int\n"
    "extern pure g(n : Int) : Int\n"
    "def always := f(y)\n"
    "def sometimes := g(y)\n"
    "def always_builtin := acc(y)\n"
    "def sometimes_builtin := accp(y)\n"
  };
  // clang-format on

  auto script = make_script(input);
  int f_hits{0};
  script.implement("f", [&f_hits](auto args) -> mold::Value {
    std::println("f");
    f_hits++;
    return {args[0]};
  });
  int g_hits{0};
  script.implement("g", [&g_hits](auto args) -> mold::Value {
    std::println("g");
    g_hits++;
    return {args[0]};
  });

  script.feed("y", {10});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  script.feed("y", {10});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  script.feed("y", {15});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  assert(f_hits == 3);
  assert(g_hits == 2);
  assert(acc_val == 35);
  assert(acc_valp == 25);
}
