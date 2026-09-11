#pragma once

#include "ast.hpp"
#include "sort.hpp"
#include <optional>
#include <unordered_map>

#define TYPED_NODE_KIND_LIST(F, R)                                             \
  NODE_KIND_LIST(F, R)                                                         \
  R(Cast)                                                                      \
  R(Inline)

struct Cast {
  NodeId child;
};

struct Inline {
  std::string_view callee;
  std::vector<NodeId> params;
  NodeId formula;
};

#define TYPE_BASE_LIST(X) X(INT) X(REAL) X(BOOL) X(STRING) X(DATE) X(TIME) X(EVENT)

struct MoldType {
#define X(T) T,
  enum Base { FAIL, TYPE_BASE_LIST(X) } base;
#undef X
  bool nullable;

  std::string show() const;
};

class TypedNodePool;

struct TypedNode {
#define F(N) N
#define R(N) , N
  using Var = std::variant<TYPED_NODE_KIND_LIST(F, R)>;
#undef F
#undef R
  Var data;
  Source source;

  MoldType type;

  std::string show(const TypedNodePool &pool) const;
};

class TypedNodePool {
public:
  NodeId push(TypedNode n) {
    nodes.push_back(std::move(n));
    return {static_cast<std::uint32_t>(nodes.size() - 1)};
  }

  TypedNode &operator[](NodeId id) { return nodes[id.id]; }
  const TypedNode &operator[](NodeId id) const { return nodes[id.id]; }

  NodeId begin() const { return {0}; }
  NodeId end() const { return {static_cast<NodeId::Type>(nodes.size())}; }

  NodeId::Type size() const { return nodes.size(); }

private:
  std::vector<TypedNode> nodes;
};

struct TypedAST {
  TypedNodePool pool;
  std::vector<NodeId> tls;

  TypedNode &operator[](NodeId id) { return pool[id]; }
  const TypedNode &operator[](NodeId id) const { return pool[id]; }

  NodeId begin() const { return pool.begin(); }
  NodeId end() const { return pool.end(); }
  NodeId::Type size() const { return pool.size(); }
};

struct UFNode {
  std::uint32_t parent;
  std::optional<MoldType::Base> base;
};

struct InternType {
  union {
    MoldType::Base base;
    std::uint32_t var_id;
  };
  bool is_var;
  bool nullable;

  static InternType concrete(MoldType::Base base, bool n = false) {
    return {{.base = base}, false, n};
  }

  static InternType var(std::uint32_t id, bool n = false) {
    return {{.var_id = id}, true, n};
  }
};

struct Constraint {
  std::vector<InternType> vars;
  std::vector<std::vector<MoldType>> cases;
};

struct Signature {
  std::vector<InternType> params;
  InternType ret;
};

class Typing {
public:
  explicit Typing(const AST &ast, const SortResult &sorting)
      : ast(ast), sorting(sorting) {}

  TypedAST run();

private:
  const AST &ast;
  const SortResult &sorting;

  std::vector<UFNode> uf{};
  std::vector<Constraint> ctrs{};
  std::vector<std::pair<std::string_view, InternType>> locals{};
  std::unordered_map<std::string_view, InternType> globals{};
  std::unordered_map<std::string_view, Signature> sigs{};

  std::vector<InternType> inferred{};
  TypedNodePool pool{};

  std::uint32_t fresh();
  std::uint32_t find(std::uint32_t i);
  [[nodiscard]]
  bool unify(InternType a, InternType b);

  InternType join(InternType a, InternType b);

  bool compatible(InternType have, MoldType exp);

  bool propagating{false};
  void propagate();

  std::optional<MoldType::Base> get_base(InternType t);
  MoldType to_mold_type(InternType t);

  NodeId push_node(TypedNode::Var var, Source src, InternType t);

  InternType lookup(std::string_view name);

  bool assignable(InternType arg, InternType param);

  NodeId infer(NodeId id);
};
