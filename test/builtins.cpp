#include "common.hpp"
#include <string>
#include <thread>

int main() {
  using namespace std::chrono_literals;

  // clang-format off
  std::string input {
    "def len := String.length(foo)\n"
    "in foo : String\n"
    "def bar := Date.now()\n"
  };
  // clang-format on

  auto script = make_script(input);

  script.feed("foo", {"Heyya"});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }
  assert(std::get<std::int64_t>(script.read("len").data) == 5);
  auto d1 = std::get<mold::Date>(script.read("bar").data);
  std::println("len = {}", script.read("len"));
  std::println("date = {}", script.read("bar"));

  std::this_thread::sleep_for(200ms);

  script.feed("foo", {"Hello world!"});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }
  assert(std::get<std::int64_t>(script.read("len").data) == 12);
  auto d2 = std::get<mold::Date>(script.read("bar").data);
  std::println("len = {}", script.read("len"));
  std::println("date = {}", script.read("bar"));

  assert(d2 - d1 >= 190ms);
}
