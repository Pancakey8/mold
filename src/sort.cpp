#include "sort.hpp"
#include "ast.hpp"
#include "diagnostics.hpp"
#include "utils.hpp"
#include <queue>
#include <unordered_map>

namespace mold::internal {

void TopoSort::deps_of(NodeId node, std::vector<std::string_view> &deps,
                       std::vector<std::string_view> &locals) {
  std::visit(overload{[&](const LitInt &) {}, [&](const LitReal &) {},
                      [&](const LitBool &) {}, [&](const LitString &) {},
                      [&](const LitNull &) {},
                      [&](const Ident &n) {
                        if (std::find(locals.begin(), locals.end(), n.name) ==
                            locals.end()) {
                          deps.push_back(n.name);
                          if (!tls.contains(n.name)) {
                            diags.emplace_back("Reference to undefined name",
                                               ast[node].source);
                          }
                        }
                      },
                      [&](const BinaryOp &n) {
                        deps_of(n.left, deps, locals);
                        deps_of(n.right, deps, locals);
                      },
                      [&](const LetIn &n) {
                        deps_of(n.init, deps, locals);
                        locals.push_back(n.name);
                        deps_of(n.body, deps, locals);
                        locals.pop_back();
                      },
                      [&](const IfElse &n) {
                        deps_of(n.cond, deps, locals);
                        deps_of(n.tru, deps, locals);
                        if (n.fals != NODEID_NONE)
                          deps_of(n.fals, deps, locals);
                      },
                      [&](const PreVal &) {},
                      [&](const FuncCall &n) {
                        deps.push_back(n.name);
                        for (const auto &p : n.params) {
                          deps_of(p, deps, locals);
                        }
                      },
                      [&](const TypeName &) {}, [&](const Input &) {},
                      [&](const Output &) {},
                      [&](const Formula &n) { deps_of(n.init, deps, locals); },
                      [&](const Signal &n) { deps_of(n.init, deps, locals); },
                      [&](const Extern &) {},
                      [&](const Function &n) {
                        for (const auto &p : n.params) {
                          locals.push_back(p);
                        }
                        deps_of(n.init, deps, locals);
                        locals.resize(locals.size() - n.params.size());
                      },
                      [&](const Error &) {}},
             ast[node].data);
}

std::pair<SortResult, std::vector<Diagnostic>> TopoSort::run() {
  std::flat_map<NodeId, std::vector<NodeId>> graph{};
  std::flat_map<NodeId, NodeId::Type> degrees{};

  for (auto id : ast.tls) {
    const auto &node = ast[id];
    if (!node.is_toplevel()) // Errors
      continue;
    auto name = node.toplevel_name();
    if (tls.contains(name)) {
      diags.emplace_back("Redefinition of existing top-level name",
                         node.source);
      continue;
    }
    tls.insert({name, id});
    graph[id] = {};
    degrees[id] = 0;
  }

  for (auto id : ast.tls) {
    std::vector<std::string_view> deps{}, locals{};
    deps_of(id, deps, locals);
    for (const auto &dep : deps) {
      auto p = tls.find(dep);
      if (p == tls.end()) {
        continue;
      }
      graph[p->second].push_back(id);
      degrees[id]++;
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

    for (auto p : graph[id]) {
      --degrees[p];
      if (degrees[p] == 0)
        work.push(p);
    }
  }

  if (order.size() != degrees.size()) {
    for (auto [id, deg] : degrees) {
      if (deg == 0)
        continue;
      diags.emplace_back("Cyclic formula", ast[id].source);
    }
  }

  return std::make_pair(
      SortResult{std::move(tls), std::move(graph), std::move(order)},
      std::move(diags));
}

}; // namespace mold::internal
