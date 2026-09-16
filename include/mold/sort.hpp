#pragma once

#include "ast.hpp"
#include "diagnostics.hpp"
#include <unordered_map>

namespace mold::internal {

void deps_of(const AST &ast, NodeId node, std::vector<std::string_view> &deps,
             std::vector<std::string_view> &locals);

struct SortResult {
  std::unordered_map<std::string_view, NodeId> tls_names{};
  std::flat_map<NodeId, std::vector<NodeId>> deps{};
  std::vector<NodeId> order{};
};

std::pair<SortResult, std::vector<Diagnostic>> topo_sort(const AST &ast);

class TopoSort {
public:
  TopoSort(const AST &ast) : ast(ast) {}
  std::pair<SortResult, std::vector<Diagnostic>> run();

private:
  const AST &ast;
  std::unordered_map<std::string_view, NodeId> tls{};
  std::vector<Diagnostic> diags{};

  void deps_of(NodeId node, std::vector<std::string_view> &deps,
               std::vector<std::string_view> &locals);

};

}; // namespace mold::internal
