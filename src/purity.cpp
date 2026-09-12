#include "purity.hpp"
#include "ast.hpp"
#include <cstddef>

std::vector<std::string_view>
impure_fns(const AST &ast, const std::vector<NodeId> &order,
           const std::flat_map<NodeId, std::vector<NodeId>> &deps) {
  std::vector<bool> is_impure(ast.size(), false);

  for (auto id : order) {
    if (auto ex = std::get_if<Extern>(&ast[id].data)) {
      if (!ex->pure) {
        is_impure[id.id] = true;
      }
    }
  }

  for (auto id : order) {
    if (is_impure[id.id]) {
      for (auto dependent : deps.at(id)) {
        is_impure[dependent.id] = true;
      }
    }
  }

  std::vector<std::string_view> impures;
  for (auto id : order) {
    if (is_impure[id.id]) {
      impures.push_back(ast[id].toplevel_name());
    }
  }

  return impures;
}
