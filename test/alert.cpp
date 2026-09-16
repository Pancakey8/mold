#include "common.hpp"
#include "mold/script.hpp"
#include <chrono>
#include <string>
#include <thread>

int main() {
  // clang-format off
  std::string input {
    "signal decider := if should_alert then alert(\"now!\")\n"
    "def should_alert := y /= pre(y)\n"
    "def y :=\n"
    "  if now() - pre(y) > seconds(1)\n"
    "  then now()\n"
    "  else pre(y) ? now()\n"
    "extern impure now() : Date\n"
    "extern pure seconds(n : Int) : Time\n"
    "out alert(msg : String)\n"
  };
  // clang-format on

  auto script = make_script(input);
  int hits{0};

  script.implement("now", [](auto) -> mold::Value {
    return {std::chrono::system_clock::now()};
  });
  script.implement("seconds", [](auto args) -> mold::Value {
    return {std::chrono::seconds(std::get<std::int64_t>(args[0].data))};
  });
  script.on("alert", [&](auto args) {
    std::println("here");
    assert(std::get<std::string>(args[0].data) == "now!");
    hits++;
  });

  int i = 0;
  for (; i < 32; ++i) {
    using namespace std::chrono_literals;
    if (auto res = script.tick(); !res.has_value()) {
      std::println("{}", res.error());
      assert(false && "Interpreter crash");
    }
    if (hits == 2) break;
    std::this_thread::sleep_for(100ms);
  }

  // Potentially not stable if precise, but i == 10 is expected
  std::println("{}", i);
  assert(hits == 2 && i > 5);
}
