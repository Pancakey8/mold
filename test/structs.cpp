#include "common.hpp"
#include <string>
int main() {
  // clang-format off
  std::string input {
    "in n : Int\n"
    "struct Foo { x : Int, y : String }\n"
    "def x := Foo { y := \"Hello\", x := n }\n"
    "def k : Foo := Foo { y := x->y, x := n * 2 }\n"
  };
  // clang-format on

  auto script = make_script(input);

  script.feed("n", {3});
  if (auto res = script.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  std::println("x = {} | k = {}", script.read("x"), script.read("k"));

  auto x = std::get<mold::Struct>(script.read("x").data);
  assert(x.kind == "Foo");
  assert(std::get<std::int64_t>(x.fields[0].data) == 3);
  assert(std::get<std::string>(x.fields[1].data) == "Hello");
  auto k = std::get<mold::Struct>(script.read("k").data);
  assert(k.kind == "Foo");
  assert(std::get<std::int64_t>(k.fields[0].data) == 6);
  assert(std::get<std::string>(k.fields[1].data) == "Hello");
}
