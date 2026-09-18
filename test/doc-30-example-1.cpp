#include "common.hpp"
#include "mold/script.hpp"
#include <chrono>
#include <fstream>
#include <print>
#include <ranges>
#include <sstream>
#include <string>
#include <thread>

int main() {
  std::ifstream file("test/doc-30-example-1.mold");
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string input = buffer.str();
  std::println("{}", input);

  std::vector<std::int64_t> temps{65, 66, 65, 65, 71, 77, 78, 79};

  auto script = make_script(input);

  enum Decision { ALERT0, ALERT1, LOG, NONE };
  std::vector<Decision> decs{LOG, LOG, LOG, NONE, ALERT0, NONE, ALERT1, NONE};

  Decision dec{};

  script.on("alert", [&dec](auto args) {
    if (std::get<std::string>(args[0].data) == "Overheating fast") {
      dec = ALERT0;
    } else {
      dec = ALERT1;
    }
    std::println("[SEVERE] {}", args[0]);
  });

  script.on("log", [&dec](auto args) {
    dec = LOG;
    std::println("[INFO] {} -- {}℃", args[0], args[1]);
  });

  for (auto [exp, temp] : std::views::zip(decs, temps)) {
    dec = NONE;
    script.feed("temp", {temp});
    if (auto res = script.tick(); !res.has_value()) {
      std::println("{}", res.error());
      assert(false && "Interpreter crash");
    }
    assert(exp == dec);
  }
}
