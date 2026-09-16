#include "mold/ranking.hpp"
#include "mold/ast.hpp"
#include "mold/typing.hpp"
#include "mold/utils.hpp"
#include <unordered_map>
#include <variant>

namespace mold::internal {

std::flat_map<NodeId, std::size_t> ranks_of(const SortResult &sort) {
  std::flat_map<NodeId, std::size_t> rank{};

  for (auto id : sort.order)
    rank[id] = 0;

  for (auto id : sort.order) {
    const auto next = rank[id] + 1;
    if (auto it = sort.deps.find(id); it != sort.deps.end()) {
      for (auto dep : it->second) {
        rank[dep] = std::max(rank[dep], next);
      }
    }
  }

  return rank;
}

}; // namespace mold::internal
