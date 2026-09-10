#include "sort.hpp"
#include "utils.hpp"
#include <queue>
#include <unordered_map>

void deps_of(const AST &ast, NodeId node, std::vector<std::string_view> &deps,
             std::vector<std::string_view> &locals) {
  std::visit(
      overload{[&](const LitInt &) {}, [&](const LitReal &) {},
               [&](const LitBool &) {}, [&](const LitString &) {},
               [&](const LitNull &) {},
               [&](const Ident &n) {
                 if (std::find(locals.begin(), locals.end(), n.name) ==
                     locals.end()) {
                   deps.push_back(n.name);
                 }
               },
               [&](const BinaryOp &n) {
                 deps_of(ast, n.left, deps, locals);
                 deps_of(ast, n.right, deps, locals);
               },
               [&](const LetIn &n) {
                 deps_of(ast, n.init, deps, locals);
                 locals.push_back(n.name);
                 deps_of(ast, n.body, deps, locals);
                 locals.pop_back();
               },
               [&](const IfElse &n) {
                 deps_of(ast, n.cond, deps, locals);
                 deps_of(ast, n.tru, deps, locals);
                 deps_of(ast, n.fals, deps, locals);
               },
               [&](const PreVal &) {},
               [&](const FuncCall &n) {
                 deps.push_back(n.name);
                 for (const auto &p : n.params) {
                   deps_of(ast, p, deps, locals);
                 }
               },
               [&](const TypeName &) {}, [&](const Input &) {},
               [&](const Output &) {},
               [&](const Formula &n) { deps_of(ast, n.init, deps, locals); },
               [&](const Signal &n) { deps_of(ast, n.init, deps, locals); },
               [&](const Extern &) {},
               [&](const Function &n) {
                 for (const auto &p : n.params) {
                   locals.push_back(p);
                 }
                 deps_of(ast, n.init, deps, locals);
                 locals.resize(locals.size() - n.params.size());
               },
               [&](const Error &) {}},
      ast[node].data);
}

SortResult topo_sort(const AST &ast) {
  std::unordered_map<std::string_view, NodeId> tls{};
  std::flat_map<NodeId, std::vector<NodeId>> graph{};
  std::flat_map<NodeId, NodeId::Type> degrees{};

  for (auto id : ast.tls) {
    const auto &node = ast[id];
    auto name = node.toplevel_name();
    if (tls.contains(name))
      assert(false && "TODO: Error handling");
    tls.insert({name, id});
    graph[id] = {};
    degrees[id] = 0;
  }

  for (auto id : ast.tls) {
    std::vector<std::string_view> deps{}, locals{};
    deps_of(ast, id, deps, locals);
    for (const auto &dep : deps) {
      auto p = tls.find(dep);
      if (p == tls.end())
        assert(false && "TODO: Error handling");
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

  if (order.size() != degrees.size())
    assert(false && "TODO: Error handling");

  return {std::move(tls), std::move(graph), std::move(order)};
}
