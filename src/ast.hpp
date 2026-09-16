#pragma once

#include "lexer.hpp"
#include <cassert>
#include <cstdint>
#include <flat_map>
#include <format>
#include <vector>

namespace mold::internal {

struct NodeId {
  using Type = std::uint32_t;

  Type id;

  bool operator==(const NodeId &other) const = default;
  bool operator<(const NodeId &other) const { return id < other.id; }
  NodeId &operator++() {
    ++id;
    return *this;
  }
};

constexpr NodeId NODEID_NONE{UINT32_MAX};

struct LitInt {
  std::int64_t val;
};

struct LitReal {
  double val;
};

struct LitBool {
  bool val;
};

struct LitString {
  std::string_view val;
};

struct LitNull {};

struct Ident {
  std::string_view name;
};

#define BINARYOP_KIND_LIST(X)                                                  \
  X(AND)                                                                       \
  X(OR)                                                                        \
  X(BITAND)                                                                    \
  X(BITOR)                                                                     \
  X(EQ)                                                                        \
  X(NEQ)                                                                       \
  X(LT)                                                                        \
  X(GT)                                                                        \
  X(LE)                                                                        \
  X(GE)                                                                        \
  X(SHL)                                                                       \
  X(SHR)                                                                       \
  X(ADD)                                                                       \
  X(SUB)                                                                       \
  X(MULT)                                                                      \
  X(DIV)                                                                       \
  X(MOD)                                                                       \
  X(COAL)

struct BinaryOp {
  enum Kind {
#define X(K) K,
    BINARYOP_KIND_LIST(X)
#undef X
  } kind;
  NodeId left, right;

  static std::string op_str(BinaryOp::Kind kind);
};

struct LetIn {
  std::string_view name;
  NodeId init, body;
};

struct IfElse {
  NodeId cond, tru, fals;
};

struct PreVal {
  std::string_view name;
  std::uint32_t depth;
};

struct FuncCall {
  std::string_view name;
  std::vector<NodeId> params;
};

struct TypeName {
  std::string_view base;
  bool nullable;
};

struct Input {
  std::string_view name;
  NodeId type;
};

struct Output {
  std::string_view name;
  std::vector<std::pair<std::string_view, NodeId>> params;
};

struct Formula {
  std::string_view name;
  NodeId init;
};

struct Signal {
  std::string_view name;
  NodeId init;
};

struct Extern {
  std::string_view name;
  std::vector<std::pair<std::string_view, NodeId>> params;
  NodeId ret;
  bool pure;
};

struct Function {
  std::string_view name;
  std::vector<std::string_view> params;
  NodeId init;
};

struct Error {
  std::string_view msg;
};

class NodePool;

#define NODE_KIND_LIST(F, R)                                                   \
  F(LitInt)                                                                    \
  R(LitReal)                                                                   \
  R(LitBool)                                                                   \
  R(LitString)                                                                 \
  R(LitNull)                                                                   \
  R(Ident)                                                                     \
  R(BinaryOp)                                                                  \
  R(LetIn)                                                                     \
  R(IfElse)                                                                    \
  R(PreVal)                                                                    \
  R(FuncCall)                                                                  \
  R(TypeName)                                                                  \
  R(Input)                                                                     \
  R(Output)                                                                    \
  R(Formula)                                                                   \
  R(Signal)                                                                    \
  R(Extern)                                                                    \
  R(Function)                                                                  \
  R(Error)

struct Node {
#define F(N) N
#define R(N) , N
  using Var = std::variant<NODE_KIND_LIST(F, R)>;
#undef F
#undef R
  Var data;
  Source source;

  template <typename T>
  constexpr static bool is_toplevel_v =
      std::is_same_v<T, Input> || std::is_same_v<T, Output> ||
      std::is_same_v<T, Formula> || std::is_same_v<T, Signal> ||
      std::is_same_v<T, Extern> || std::is_same_v<T, Function>;

  bool is_toplevel() const;

  std::string_view toplevel_name() const;

  std::string show(const NodePool &pool) const;
};

class NodePool {
public:
  NodeId push(Node n) {
    nodes.push_back(std::move(n));
    return {static_cast<std::uint32_t>(nodes.size() - 1)};
  }

  Node &operator[](NodeId id) { return nodes[id.id]; }
  const Node &operator[](NodeId id) const { return nodes[id.id]; }

  NodeId begin() const { return {0}; }
  NodeId end() const { return {static_cast<NodeId::Type>(nodes.size())}; }

  NodeId::Type size() const { return nodes.size(); }

private:
  std::vector<Node> nodes;
};

struct AST {
  NodePool pool;
  std::vector<NodeId> tls;

  Node &operator[](NodeId id) { return pool[id]; }
  const Node &operator[](NodeId id) const { return pool[id]; }

  NodeId begin() const { return pool.begin(); }
  NodeId end() const { return pool.end(); }
  NodeId::Type size() const { return pool.size(); }
};
}; // namespace mold::internal

template <> struct std::formatter<mold::internal::NodeId> {
  constexpr auto parse(std::format_parse_context &ctx) const {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const mold::internal::NodeId &id, FormatContext &ctx) const {
    return std::format_to(ctx.out(), "Node({})", id.id);
  }
};
