#include "mold/ranking.hpp"
#include "mold/ast.hpp"
#include "mold/builtins.hpp"
#include "mold/typing.hpp"
#include "mold/utils.hpp"
#include <algorithm>
#include <flat_map>
#include <queue>
#include <string_view>
#include <variant>
#include <vector>

namespace mold::internal {

bool is_impure_builtin(std::string_view name) {
  if (auto idx = is_builtin(name)) {
    return !get_builtins()[*idx].is_pure;
  }
  return false;
}

void typed_deps(NodeId node, const TypedAST &ast,
                std::vector<std::string_view> &deps,
                std::vector<std::string_view> &locals) {
  if (node == NODEID_NONE)
    return;

  std::visit(
      overload{
          [&](const LitInt &) {},
          [&](const LitReal &) {},
          [&](const LitBool &) {},
          [&](const LitString &) {},
          [&](const LitNull &) {},
          [&](const Ident &n) {
            if (std::find(locals.begin(), locals.end(), n.name) ==
                locals.end()) {
              deps.push_back(n.name);
            }
          },
          [&](const BinaryOp &n) {
            typed_deps(n.left, ast, deps, locals);
            if (n.kind != BinaryOp::MEMB) {
              typed_deps(n.right, ast, deps, locals);
            }
          },
          [&](const LetIn &n) {
            typed_deps(n.init, ast, deps, locals);
            locals.push_back(n.name);
            typed_deps(n.body, ast, deps, locals);
            locals.pop_back();
          },
          [&](const IfElse &n) {
            typed_deps(n.cond, ast, deps, locals);
            typed_deps(n.tru, ast, deps, locals);
            typed_deps(n.fals, ast, deps, locals);
          },
          [&](const PreVal &) {},
          [&](const FuncCall &n) {
            deps.push_back(n.name);
            for (auto p : n.params) {
              typed_deps(p, ast, deps, locals);
            }
          },
          [&](const StructConst &n) {
            for (auto [k, p] : n.inits) {
              typed_deps(p, ast, deps, locals);
            }
          },
          [&](const TypeName &) {},
          [&](const Input &) {},
          [&](const Output &) {},
          [&](const Formula &n) { typed_deps(n.init, ast, deps, locals); },
          [&](const Signal &n) { typed_deps(n.init, ast, deps, locals); },
          [&](const Extern &) {},
          [&](const Function &n) {
            for (const auto &p : n.params) {
              locals.push_back(p);
            }
            typed_deps(n.init, ast, deps, locals);
            locals.resize(locals.size() - n.params.size());
          },
          [&](const StructDef &) {},
          [&](const Error &) {},
          [&](const Inline &n) {
            for (auto p : n.params) {
              typed_deps(p, ast, deps, locals);
            }
            for (auto name : n.names) {
              locals.push_back(name);
            }
            typed_deps(n.formula, ast, deps, locals);
            locals.resize(locals.size() - n.names.size());
          },
      },
      ast[node].data);
}

Ranking ranks_of(const TypedAST &ast) {
  std::flat_map<std::string_view, NodeId> tls{};
  std::flat_map<NodeId, std::vector<NodeId>> graph{};
  std::flat_map<NodeId, NodeId::Type> degrees{};
  std::flat_map<NodeId, char> impure_builtin_calls{};

  for (auto id : ast.tls) {
    if (id == NODEID_NONE)
      continue;
    const auto &node = ast[id];
    if (!node.is_toplevel())
      continue;

    auto name = node.toplevel_name();
    tls[name] = id;
    graph[id] = {};
    degrees[id] = 0;
    impure_builtin_calls[id] = false;
  }

  for (auto id : ast.tls) {
    if (id == NODEID_NONE)
      continue;
    std::vector<std::string_view> deps{}, locals{};
    typed_deps(id, ast, deps, locals);

    for (const auto &dep : deps) {
      auto it = tls.find(dep);
      if (it != tls.end()) {
        graph[it->second].push_back(id);
        degrees[id]++;
      } else if (is_impure_builtin(dep)) {
        impure_builtin_calls[id] = true;
      }
    }
  }

  std::vector<NodeId> order{};
  std::queue<NodeId> work{};

  for (auto [n, d] : degrees) {
    if (d == 0)
      work.push(n);
  }

  while (!work.empty()) {
    auto id = work.front();
    work.pop();
    order.push_back(id);

    for (auto dependent : graph[id]) {
      --degrees[dependent];
      if (degrees[dependent] == 0) {
        work.push(dependent);
      }
    }
  }

  std::flat_map<NodeId, std::uint16_t> ranks{};
  for (auto id : order) {
    ranks[id] = 0;
  }

  for (auto id : order) {
    const bool rank_zero =
        std::visit(overload{
                       [&](const Input &) { return true; },
                       [&](const Extern &n) { return !n.pure; },
                       [&](const auto &) { return false; },
                   },
                   ast[id].data);

    if (rank_zero) {
      ranks[id] = 0;
    } else if (impure_builtin_calls[id]) {
      ranks[id] = std::max(ranks[id], static_cast<std::uint16_t>(1));
    }

    const std::uint16_t next_rank = ranks[id] + 1;
    auto it = graph.find(id);
    if (it != graph.end()) {
      for (auto dependent : it->second) {
        ranks[dependent] = std::max(ranks[dependent], next_rank);
      }
    }
  }

  return {std::move(graph), std::move(ranks), std::move(impure_builtin_calls)};
}

}; // namespace mold::internal
