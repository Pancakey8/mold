#pragma once

#include "ast.hpp"
#include <unordered_map>

namespace mold::internal {

void deps_of(const AST &ast, NodeId node, std::vector<std::string_view> &deps,
             std::vector<std::string_view> &locals);

struct SortResult {
  std::unordered_map<std::string_view, NodeId> tls_names{};
  std::flat_map<NodeId, std::vector<NodeId>> deps{};
  std::vector<NodeId> order{};
};

SortResult topo_sort(const AST &ast);

}; // namespace foo::internal
