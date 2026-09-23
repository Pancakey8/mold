#include "mold/builtins.hpp"
#include "mold/interpreter.hpp"
#include "mold/typing.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iterator>

namespace mold::internal {

InternValue builtin_str_length(std::int16_t, InternValue *argv) {
  auto len = argv[0].data.s->len;
  return {{.i = static_cast<std::int64_t>(len)}, InternValue::INT};
}

InternValue builtin_date_now(std::int16_t, InternValue *) {
  auto d = std::chrono::system_clock::now();
  auto t = std::chrono::time_point_cast<std::chrono::nanoseconds>(d)
               .time_since_epoch()
               .count();

  return {{.d = t}, InternValue::DATE};
}

InternValue builtin_int_max(std::int16_t, InternValue *argv) {
  if (argv[0].tag == InternValue::NIL) {
    argv[1].inc();
    return argv[1];
  } else if (argv[1].tag == InternValue::NIL) {
    argv[0].inc();
    return argv[0];
  }
  auto l = argv[0].data.i;
  auto r = argv[1].data.i;
  return {{.i = std::max(l, r)}, InternValue::INT};
}

InternValue builtin_real_max(std::int16_t, InternValue *argv) {
  if (argv[0].tag == InternValue::NIL) {
    argv[1].inc();
    return argv[1];
  } else if (argv[1].tag == InternValue::NIL) {
    argv[0].inc();
    return argv[0];
  }
  auto l = argv[0].data.r;
  auto r = argv[1].data.r;
  return {{.r = std::max(l, r)}, InternValue::REAL};
}

InternValue builtin_not(std::int16_t, InternValue *argv) {
  if (argv[0].tag == InternValue::NIL) {
    argv[0].inc();
    return argv[0];
  }

  auto b = argv[0].data.b;
  return {{.b = !b}, InternValue::BOOL};
}

// clang-format off
static std::vector<Builtin> BUILTINS {
#define BUILTIN(NAME, PARS, RET, PURITY, IMPL, DOC) Builtin{NAME, PARS, RET, PURITY, IMPL},
#define PARAMS(...) { __VA_ARGS__ }
#define TY(BASE, NIL) MoldType{MoldType::BASE, NIL}
#include "mold/builtins_table.h"
#undef BUILTIN
#undef PARAMS
#undef TY
};
// clang-format on

std::span<const Builtin> get_builtins() { return BUILTINS; }

std::optional<std::size_t> is_builtin(std::string_view name) {
  auto it = std::find_if(BUILTINS.begin(), BUILTINS.end(),
                         [name](auto b) { return b.name == name; });

  if (it == BUILTINS.end()) {
    return {};
  } else {
    return std::distance(BUILTINS.begin(), it);
  }
}

void add_builtin(Builtin b) { BUILTINS.push_back(std::move(b)); }

} // namespace mold::internal
