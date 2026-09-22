#include "common.hpp"
#include "mold/script.hpp"
#include <string>

int main() {
  // clang-format off
  std::string input1 {
    "struct Foo { x : Int }\n"
    "struct Bar { x : Int }\n"
    "def x := Foo { x := 3 }\n"
    "def y := Bar { x := 5 }\n"
    "def z := x = y\n"
  };
  // clang-format on

  assert(fail_compile(input1));

  // clang-format off
  std::string input2 {
    "in k : Int\n"
    "struct Foo { x : Int }\n"
    "def x := Foo { x := k + 2 }\n"
    "def y := Foo { x := k - 1 }\n"
    "def z := Foo { x := k * 2 }\n"
    "def a := x = y\n"
    "def b := x = z\n"
  };
  // clang-format on

  auto script2 = make_script(input2);

  script2.feed("k", {2});
  if (auto res = script2.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  std::println("x = {}", script2.read("x"));
  std::println("y = {}", script2.read("y"));
  std::println("z = {}", script2.read("z"));

  assert(std::get<bool>(script2.read("a").data) == false);
  assert(std::get<bool>(script2.read("b").data) == true);

  // clang-format off
  std::string input3 {
    "struct Vec2i { x : Int, y : Int }\n"
    "extern pure Vec2i.add(v : Vec2i, w : Vec2i) : Vec2i\n"

    "struct Vec2f { x : Real, y : Real }\n"
    "extern pure Vec2f.add(v : Vec2f, w : Vec2f) : Vec2f\n"

    "def isum := add(vi, wi)\n"
    "def vi := Vec2i { x := -8, y := 7 }\n"
    "def wi := Vec2i { x := 11, y := 34 }\n"

    "def fsum := add(vf, wf)\n"
    "def vf := Vec2f { x := -6.5, y := 16.5 }\n"
    "def wf := Vec2f { x := 3.5, y := 6 }\n"
  };
  // clang-format on

  auto script3 = make_script(input3);
  script3.implement("Vec2i.add", [](auto args) -> mold::Value {
    auto v = std::get<mold::Struct>(args[0].data);
    auto vx = std::get<std::int64_t>(v.fields["x"].data);
    auto vy = std::get<std::int64_t>(v.fields["y"].data);
    auto w = std::get<mold::Struct>(args[1].data);
    auto wx = std::get<std::int64_t>(w.fields["x"].data);
    auto wy = std::get<std::int64_t>(w.fields["y"].data);
    return {mold::Struct{"Vec2i", { {"x", {vx + wx}}, {"y", {vy + wy}} }}};
  });
  script3.implement("Vec2f.add", [](auto args) -> mold::Value {
    auto v = std::get<mold::Struct>(args[0].data);
    auto vx = std::get<double>(v.fields["x"].data);
    auto vy = std::get<double>(v.fields["y"].data);
    auto w = std::get<mold::Struct>(args[1].data);
    auto wx = std::get<double>(w.fields["x"].data);
    auto wy = std::get<double>(w.fields["y"].data);
    return {mold::Struct{"Vec2f", { {"x", {vx + wx}}, {"y", {vy + wy}} }}};
  });

  if (auto res = script3.tick(); !res.has_value()) {
    std::println("{}", res.error());
    assert(false && "Interpreter crash");
  }

  std::println("isum = {}", script3.read("isum"));
  auto isum = std::get<mold::Struct>(script3.read("isum").data);
  assert(isum.kind == "Vec2i");
  assert(std::get<std::int64_t>(isum.fields["x"].data) == 3);
  assert(std::get<std::int64_t>(isum.fields["y"].data) == 41);
  std::println("fsum = {}", script3.read("fsum"));
  auto fsum = std::get<mold::Struct>(script3.read("fsum").data);
  assert(fsum.kind == "Vec2f");
  assert(std::get<double>(fsum.fields["x"].data) == -3.0);
  assert(std::get<double>(fsum.fields["y"].data) == 22.5);
}
