#include "common.hpp"
#include <fstream>
#include <sstream>
#include <string>

int main() {
  std::ifstream file("test/readme-01.mold");
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string input = buffer.str();
  std::println("{}", input);

  auto script = make_script(input);
  script.on("log", [](auto args) {
    auto msg = std::get<std::string>(args[0].data);
    assert(msg == "Rapid temperature increase");
  });

  script.feed("temperature", {85.0});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  script.feed("temperature", {105.0});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  assert(std::get<double>(script.read("change").data) == 20.0);
}
