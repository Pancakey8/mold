#pragma once

#include <cstdint>
#include <format>
#include <memory>
#include <type_traits>
#include <variant>

namespace mold {

struct Value {
  std::variant<std::monostate, std::int64_t, double, bool, std::string> data;
};

class Script {
public:
  static Script of_string(std::string_view input);

  void feed(std::string_view name, Value value);

  void tick();

  Value read(std::string_view name);

  ~Script();

private:
  struct Impl;
  std::unique_ptr<Impl> impl;

  explicit Script(std::unique_ptr<Impl> impl);
};

} // namespace mold

template <> struct std::formatter<mold::Value> {
  constexpr auto parse(std::format_parse_context &ctx) const {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const mold::Value &v, FormatContext &ctx) const {
    return std::visit(
        [&ctx](auto &&data) {
          using T = std::decay_t<decltype(data)>;
          if constexpr (std::is_same_v<T, std::monostate>) {
            return std::format_to(ctx.out(), "null");
          } else {
            return std::format_to(ctx.out(), "{}", data);
          }
        },
        v.data);
  }
};
