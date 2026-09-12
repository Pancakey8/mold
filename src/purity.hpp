#pragma once

#include "ast.hpp"
#include <vector>

std::vector<std::string_view>
impure_fns(const AST &ast, const std::vector<NodeId> &order,
           const std::flat_map<NodeId, std::vector<NodeId>> &deps);
