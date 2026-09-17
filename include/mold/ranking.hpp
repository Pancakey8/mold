#pragma once

#include "sort.hpp"
#include "typing.hpp"
#include <cstddef>
#include <cstdint>
#include <flat_map>

namespace mold::internal {

struct Ranking {
  std::flat_map<NodeId, std::vector<NodeId>> deps;
  std::flat_map<NodeId, std::uint16_t> ranks;
  std::flat_map<NodeId, char> builtins{};
};

Ranking ranks_of(const TypedAST &ast);

}; // namespace mold::internal
