#pragma once

#include <string_view>
#include "lexer.hpp"

namespace mold::internal {

struct Diagnostic {
  std::string_view message;
  Source src;
};

} // namespace mold::internal
