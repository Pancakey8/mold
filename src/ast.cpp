#include "ast.hpp"
#include "utils.hpp"

namespace mold::internal {
bool Node::is_toplevel() const {
  return std::visit(
      [](const auto &node) {
        return is_toplevel_v<std::decay_t<decltype(node)>>;
      },
      data);
}

std::string_view Node::toplevel_name() const {
  return std::visit(
      [](const auto &node) -> std::string_view {
        using T = std::decay_t<decltype(node)>;
        if constexpr (is_toplevel_v<T>) {
          return node.name;
        } else {
          assert(false && "Name of non-top-level requested");
        }
      },
      data);
}

std::string BinaryOp::op_str(BinaryOp::Kind kind) {
#define X(K)                                                                   \
  case BinaryOp::K:                                                            \
    return #K;
  switch (kind) { BINARYOP_KIND_LIST(X) }
#undef X
}

std::string node_str(NodeId id, const NodePool &pool) {
  if (id == NODEID_NONE) {
    return "none";
  }
  return pool[id].show(pool);
}

std::string node_vec_str(const std::vector<NodeId> &vec, const NodePool &pool) {
  std::string res = "[";
  for (std::size_t i = 0; i < vec.size(); ++i) {
    if (i > 0)
      res += ", ";
    res += node_str(vec[i], pool);
  }
  res += "]";
  return res;
}

std::string string_vec_str(const std::vector<std::string_view> &vec) {
  std::string res = "[";
  for (std::size_t i = 0; i < vec.size(); ++i) {
    if (i > 0)
      res += ", ";
    res += vec[i];
  }
  res += "]";
  return res;
}

std::string map_str(const std::flat_map<std::string_view, NodeId> &m,
                    const NodePool &pool) {
  std::string res = "{";
  bool first = true;
  for (const auto &[k, v] : m) {
    if (!first)
      res += ", ";
    res += std::format("{}={}", k, node_str(v, pool));
    first = false;
  }
  res += "}";
  return res;
}

std::string Node::show(const NodePool &pool) const {
  std::string_view tag;
#define X(N, I)                                                                \
  I(std::holds_alternative<N>(data)) { tag = #N; }
#define F(N) X(N, if)
#define R(N) X(N, else if)
  NODE_KIND_LIST(F, R)
#undef X
#undef F
#undef R

  std::string fields = std::visit(
      overload{
          [&](const LitInt &n) { return std::format("val={}", n.val); },
          [&](const LitReal &n) { return std::format("val={}", n.val); },
          [&](const LitBool &n) { return std::format("val={}", n.val); },
          [&](const LitString &n) { return std::format("val={}", n.val); },
          [&](const LitNull &) { return std::string(); },
          [&](const Ident &n) { return std::format("name={}", n.name); },
          [&](const BinaryOp &n) {
            return std::format("kind={}, left={}, right={}",
                               BinaryOp::op_str(n.kind), node_str(n.left, pool),
                               node_str(n.right, pool));
          },
          [&](const LetIn &n) {
            return std::format("name={}, init={}, body={}", n.name,
                               node_str(n.init, pool), node_str(n.body, pool));
          },
          [&](const IfElse &n) {
            return std::format("cond={}, tru={}, fals={}",
                               node_str(n.cond, pool), node_str(n.tru, pool),
                               node_str(n.fals, pool));
          },
          [&](const PreVal &n) {
            return std::format("name={}, depth={}", n.name, n.depth);
          },
          [&](const FuncCall &n) {
            return std::format("name={}, params={}", n.name,
                               node_vec_str(n.params, pool));
          },
          [&](const TypeName &n) {
            return std::format("base={}, nullable={}", n.base, n.nullable);
          },
          [&](const Input &n) {
            return std::format("name={}, type={}", n.name,
                               node_str(n.type, pool));
          },
          [&](const Output &n) {
            return std::format("name={}, params={}", n.name,
                               map_str(n.params, pool));
          },
          [&](const Formula &n) {
            return std::format("name={}, init={}", n.name,
                               node_str(n.init, pool));
          },
          [&](const Signal &n) {
            return std::format("name={}, init={}", n.name,
                               node_str(n.init, pool));
          },
          [&](const Extern &n) {
            return std::format("name={}, params={}, ret={}, pure={}", n.name,
                               map_str(n.params, pool), node_str(n.ret, pool),
                               n.pure);
          },
          [&](const Function &n) {
            return std::format("name={}, params={}, init={}", n.name,
                               string_vec_str(n.params),
                               node_str(n.init, pool));
          },
          [&](const Error &n) { return std::format("msg={}", n.msg); }},
      data);

  return std::format("{}({})", tag, fields);
}
}; // namespace foo::internal
