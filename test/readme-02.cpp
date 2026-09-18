#include "common.hpp"
#include "mold/script.hpp"
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

int main() {
  std::ifstream file("test/readme-02.mold");
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string input = buffer.str();
  std::println("{}", input);

  enum Toggle { NOPE, ON, OFF };
  int tick{0};
  std::vector<double> temps{15.1, 15.3, 15.6, 21.1, 26.3, 32.2, 25.5};
  std::vector<bool> alerts{false, false, false, true, true, true, false};
  std::vector<Toggle> toggles{OFF, NOPE, NOPE, ON, NOPE, NOPE, OFF};

  auto script = make_script(input);

  std::vector<bool> alerted(7, false);
  script.on("alert", [&alerted, &tick](auto args) {
    auto current = std::get<double>(args[0].data);
    auto when = std::get<mold::Date>(args[1].data);
    std::println("(!) {}℃ -- {}", current, when);
    alerted[tick] = true;
  });

  std::vector<Toggle> toggled_ac(7, NOPE);
  script.on("toggle_ac", [&toggled_ac, &tick](auto args) {
    auto on_off = std::get<bool>(args[0].data);
    if (on_off) {
      std::println("Opened AC");
      toggled_ac[tick] = ON;
    } else {
      std::println("Closed AC");
      toggled_ac[tick] = OFF;
    }
  });

  script.implement("now", [](auto) -> mold::Value {
    return {std::chrono::system_clock::now()};
  });

  script.implement("milli", [](auto args) -> mold::Value {
    auto n = std::get<std::int64_t>(args[0].data);
    return {std::chrono::milliseconds(n)};
  });

  for (auto temp : temps) {
    using namespace std::chrono_literals;
    script.feed("temperature", {temp});
    if (auto res = script.tick(); !res.has_value()) {
      std::println("{}", res.error());
      assert(false && "Interpreter crash");
    }
    std::println("Temp = {}", script.read("temperature"));
    std::this_thread::sleep_for(100ms);
    ++tick;
  }

  assert(alerted == alerts);
  assert(toggled_ac == toggles);
}
