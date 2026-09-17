#pragma once

#include "mold/interpreter.hpp"
#include "mold/typing.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace mold::internal {

struct Builtin {
  std::string_view name;
  std::vector<MoldType> params;
  MoldType ret;
  bool is_pure;
  std::function<InternValue(std::uint16_t, InternValue *)> impl;
};

std::span<const Builtin> get_builtins();

std::optional<std::size_t> is_builtin(std::string_view name);

void add_builtin(Builtin b);

} // namespace mold::internal
