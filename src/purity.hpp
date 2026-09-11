#pragma once

#include "ast.hpp"
#include <vector>

std::vector<NodeId>
impure_fns(const AST &ast, const std::vector<NodeId> &order,
           const std::flat_map<NodeId, std::vector<NodeId>> &deps);
