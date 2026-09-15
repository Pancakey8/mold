#pragma once

#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <type_traits>
#include <variant>
#include <vector>

namespace mold {

struct Value;

struct Event {
  std::string_view kind;
  std::vector<Value> args;
};

using Date = std::chrono::sys_time<std::chrono::nanoseconds>;
using Time = std::chrono::nanoseconds;

struct Value {
  std::variant<std::monostate, std::int64_t, double, bool, std::string, Event,
               Date, Time>
      data;
};

using ExtFn = std::function<Value(std::span<const Value> args)>;
using HandlerFn = std::function<void(std::span<const Value> args)>;

class Script {
public:
  static Script of_string(std::string_view input);

  void feed(std::string_view name, Value value);

  void tick();

  Value read(std::string_view name);

  void implement(std::string_view name, ExtFn fn);

  void on(std::string_view event, HandlerFn fn);

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
          } else if constexpr (std::is_same_v<T, mold::Event>) {
            auto out = std::format_to(ctx.out(), "{}(", data.kind);
            for (std::size_t i = 0; i < data.args.size(); ++i) {
              if (i > 0) {
                out = std::format_to(out, ", ");
              }
              out = std::format_to(out, "{}", data.args[i]);
            }
            return std::format_to(out, ")");
          } else if constexpr (std::is_same_v<T, mold::Date>) {
            return std::format_to(ctx.out(), "@{:%Y-%m-%dT%H:%M:%S}", data);
          } else if constexpr (std::is_same_v<T, mold::Time>) {
            return std::format_to(ctx.out(), "{}", data);
          } else {
            return std::format_to(ctx.out(), "{}", data);
          }
        },
        v.data);
  }
};
