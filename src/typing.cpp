#include "typing.hpp"
#include "ast.hpp"
#include "utils.hpp"
#include <format>

TypedAST Typing::run() {
  std::vector<NodeId> tls{};

  for (auto id : sorting.order) {
    tls.push_back(infer(id));
  }

  propagate();

  for (auto id = pool.begin(); id != pool.end(); ++id) {
    pool[id].type = to_mold_type(inferred[id.id]);
  }

  return {std::move(pool), std::move(tls)};
}

NodeId Typing::infer(NodeId id) {
  return std::visit(
      overload{
          [&](const LitInt &n) -> NodeId {
            return push_node(n, ast[id].source,
                             InternType::concrete(MoldType::INT));
          },
          [&](const LitReal &n) -> NodeId {
            return push_node(n, ast[id].source,
                             InternType::concrete(MoldType::REAL));
          },
          [&](const LitBool &n) -> NodeId {
            return push_node(n, ast[id].source,
                             InternType::concrete(MoldType::BOOL));
          },
          [&](const LitString &n) -> NodeId {
            return push_node(n, ast[id].source,
                             InternType::concrete(MoldType::STRING));
          },
          [&](const LitNull &n) -> NodeId {
            return push_node(n, ast[id].source, InternType::var(fresh()));
          },
          [&](const Ident &n) -> NodeId {
            return push_node(n, ast[id].source, lookup(n.name));
          },
          [&](const BinaryOp &n) -> NodeId {
            auto l = infer(n.left);
            auto ty_l = inferred[l.id];
            auto r = infer(n.right);
            auto ty_r = inferred[r.id];

            bool nullable = ty_l.nullable || ty_r.nullable;

            switch (n.kind) {
            case BinaryOp::AND:
            case BinaryOp::OR: {
              if (!unify(ty_l, InternType::concrete(MoldType::BOOL)) ||
                  !unify(ty_l, InternType::concrete(MoldType::BOOL)))
                assert(false && "TODO: Error handling");
              return push_node(BinaryOp{n.kind, l, r}, ast[id].source,
                               InternType::concrete(MoldType::BOOL, nullable));
            } break;
            case BinaryOp::BITAND:
            case BinaryOp::BITOR:
            case BinaryOp::SHL:
            case BinaryOp::SHR: {
              if (!unify(ty_l, InternType::concrete(MoldType::INT)) ||
                  !unify(ty_l, InternType::concrete(MoldType::INT)))
                assert(false && "TODO: Error handling");
              return push_node(BinaryOp{n.kind, l, r}, ast[id].source,
                               InternType::concrete(MoldType::INT, nullable));
            } break;
            case BinaryOp::EQ:
            case BinaryOp::NEQ: {
              if (!unify(ty_l, ty_r))
                assert(false && "TODO: Error handling");
              return push_node(BinaryOp{n.kind, l, r}, ast[id].source,
                               InternType::concrete(MoldType::BOOL));
            } break;
            case BinaryOp::LT:
            case BinaryOp::GT:
            case BinaryOp::LE:
            case BinaryOp::GE: {
              // clang-format off
              Constraint c {
                .vars = {ty_l, ty_r},
                .cases = {
                  {{MoldType::INT, ty_l.nullable},
                   {MoldType::INT, ty_r.nullable}},
                  {{MoldType::REAL, ty_l.nullable},
                   {MoldType::REAL, ty_r.nullable}},
                }
              };
              // clang-format on
              ctrs.push_back(std::move(c));
              return push_node(BinaryOp{n.kind, l, r}, ast[id].source,
                               InternType::concrete(MoldType::BOOL));
            } break;
            case BinaryOp::ADD:
            case BinaryOp::SUB:
            case BinaryOp::MULT:
            case BinaryOp::DIV:
            case BinaryOp::MOD: {
              auto res = InternType::var(fresh(), nullable);
              // clang-format off
              Constraint c {
                .vars = {ty_l, ty_r, res},
                .cases = {
                  {{MoldType::INT, ty_l.nullable},
                   {MoldType::INT, ty_r.nullable},
                   {MoldType::INT, nullable}},
                  {{MoldType::REAL, ty_l.nullable},
                   {MoldType::REAL, ty_r.nullable},
                   {MoldType::REAL, nullable}},
                }
              };
              // clang-format on
              // TODO: Other overloads
              if (n.kind == BinaryOp::ADD) {
                c.cases.push_back({{MoldType::STRING, ty_l.nullable},
                                   {MoldType::STRING, ty_r.nullable},
                                   {MoldType::STRING, nullable}});
              }
              ctrs.push_back(c);
              return push_node(BinaryOp{n.kind, l, r}, ast[id].source, res);
            } break;
            case BinaryOp::COAL: {
              if (!ty_l.nullable || ty_r.nullable || !unify(ty_l, ty_r)) {
                assert(false && "TODO: Error handling");
              }
              return push_node(BinaryOp{n.kind, l, r}, ast[id].source, ty_r);
            } break;
            }
          },
          [&](const LetIn &n) -> NodeId {
            auto init = infer(n.init);
            locals.push_back({n.name, inferred[init.id]});
            auto body = infer(n.body);
            locals.pop_back();
            return push_node(LetIn{n.name, init, body}, ast[id].source,
                             inferred[body.id]);
          },
          [&](const IfElse &n) -> NodeId {
            auto cond = infer(n.cond);
            if (!unify(inferred[cond.id],
                       InternType::concrete(MoldType::BOOL))) {
              assert(false && "TODO: Error handling");
            }

            auto tru = infer(n.tru);
            auto fals = NODEID_NONE;
            auto ty_tru = inferred[tru.id];
            auto ty_res = ty_tru;

            if (n.fals != NODEID_NONE) {
              fals = infer(n.fals);
              auto ty_fals = inferred[fals.id];
              if (!unify(ty_tru, ty_fals))
                assert(false && "TODO: Error handling");
            } else {
              ty_res.nullable = true;
            }

            return push_node(IfElse{cond, tru, fals}, ast[id].source, ty_res);
          },
          [&](const PreVal &n) -> NodeId {
            auto t = lookup(n.name);
            t.nullable = true;
            return push_node(n, ast[id].source, t);
          },
          [&](const FuncCall &n) -> NodeId {
            assert(false && "TODO: Function call");
          },
          [&](const TypeName &n) -> NodeId {
            MoldType::Base base{MoldType::FAIL};
            if (n.base == "Int") {
              base = MoldType::INT;
            } else if (n.base == "Real") {
              base = MoldType::REAL;
            } else if (n.base == "Bool") {
              base = MoldType::BOOL;
            } else if (n.base == "String") {
              base = MoldType::STRING;
            } else if (n.base == "Date") {
              base = MoldType::DATE;
            } else if (n.base == "Time") {
              base = MoldType::TIME;
            } else if (n.base == "Event") {
              base = MoldType::EVENT;
            }
            return push_node(n, ast[id].source,
                             InternType::concrete(base, n.nullable));
          },
          [&](const Input &n) -> NodeId {
            auto sig = infer(n.type);
            auto ty_sig = inferred[sig.id];
            return push_node(Input{n.name, sig}, ast[id].source, ty_sig);
          },
          [&](const Output &n) -> NodeId { assert(false && "TODO: Output"); },
          [&](const Formula &n) -> NodeId {
            auto init = infer(n.init);
            auto ty_init = inferred[init.id];
            if (globals.contains(n.name)) {
              if (!unify(globals[n.name], ty_init))
                assert(false && "TODO: Error handling");
            }
            globals[n.name] = ty_init;
            return push_node(Formula{n.name, init}, ast[id].source, ty_init);
          },
          [&](const Signal &n) -> NodeId {
            assert(false && "TODO: Signal def");
          },
          [&](const Extern &n) -> NodeId {
            assert(false && "TODO: Extern def");
          },
          [&](const Function &n) -> NodeId {
            assert(false && "TODO: Function def");
          },
          [&](const Error &n) -> NodeId {
            return push_node(n, ast[id].source,
                             InternType::concrete(MoldType::FAIL));
          },
      },
      ast[id].data);
}

std::uint32_t Typing::fresh() {
  std::uint32_t id = uf.size();
  uf.push_back({id, {}});
  return id;
}

std::uint32_t Typing::find(std::uint32_t i) {
  if (i == uf[i].parent)
    return i;

  return (uf[i].parent = find(uf[i].parent));
}

bool Typing::unify(InternType a, InternType b) {
  if (!a.is_var && !b.is_var) {
    if (a.base == b.base) {
      propagate();
      return true;
    }
    return false;
  }

  if (a.is_var && !b.is_var) {
    std::uint32_t r = find(a.var_id);
    if (uf[r].base && *uf[r].base != b.base)
      return false;
    uf[r].base = b.base;
    propagate();
    return true;
  }

  if (!a.is_var && b.is_var) {
    std::uint32_t r = find(b.var_id);
    if (uf[r].base && *uf[r].base != a.base)
      return false;
    uf[r].base = a.base;
    propagate();
    return true;
  }

  std::uint32_t r1 = find(a.var_id);
  std::uint32_t r2 = find(b.var_id);

  if (r1 == r2) {
    propagate();
    return true;
  }

  uf[r2].parent = r1;
  if (!uf[r1].base && uf[r2].base) {
    uf[r1].base = uf[r2].base;
  } else if (uf[r1].base && uf[r2].base && *uf[r1].base != *uf[r2].base) {
    return false;
  }

  propagate();
  return true;
}

bool Typing::compatible(InternType have, MoldType exp) {
  if (!have.is_var)
    return have.base == exp.base;

  std::uint32_t r = find(have.var_id);

  if (!uf[r].base)
    return true;

  return *uf[r].base == exp.base;
}

void Typing::propagate() {
  if (propagating)
    return;

  propagating = true;

  bool changed{true};
  while (changed) {
    changed = false;

    for (auto &c : ctrs) {
      auto e = std::erase_if(c.cases, [&](const auto &alt) -> bool {
        for (std::uint32_t i = 0; i < c.vars.size(); ++i) {
          if (!compatible(c.vars[i], alt[i]))
            return true;
        }

        return false;
      });

      if (e > 0)
        changed = true;

      if (c.cases.size() == 1) {
        auto &tys = c.cases.front();
        for (std::uint32_t i = 0; i < c.vars.size(); ++i) {
          if (!unify(c.vars[i],
                     InternType::concrete(tys[i].base, tys[i].nullable))) {
            assert(false && "TODO: Error handling");
          }
        }
      }

      if (c.cases.size() == 0) {
        assert(false && "TODO: Error handling");
      }
    }
  }

  propagating = false;
}

std::optional<MoldType::Base> Typing::get_base(InternType t) {
  if (!t.is_var) {
    if (t.base == MoldType::FAIL)
      return {};
    return t.base;
  }
  return uf[find(t.var_id)].base;
}

MoldType Typing::to_mold_type(InternType t) {
  if (!t.is_var)
    return {t.base, t.nullable};
  auto b = get_base(t);
  return {b.value_or(MoldType::FAIL), t.nullable};
}

NodeId Typing::push_node(TypedNode::Var var, Source src, InternType t) {
  auto id = pool.push(TypedNode{std::move(var), src, {MoldType::FAIL, false}});

  if (id.id >= inferred.size()) {
    inferred.resize(id.id + 16);
  }

  inferred[id.id] = t;

  return id;
}

InternType Typing::lookup(std::string_view var) {
  if (auto it = std::find_if(locals.rbegin(), locals.rend(),
                             [&](const auto &p) { return p.first == var; });
      it != locals.rend()) {
    return it->second;
  }

  if (auto it = globals.find(var); it != globals.end()) {
    return it->second;
  }

  auto v = InternType::var(fresh());
  globals[var] = v;
  return v;
}

std::string MoldType::show() const {
  std::string_view base_str;
  switch (base) {
  case FAIL:
    base_str = "Fail";
    break;
  case INT:
    base_str = "Int";
    break;
  case REAL:
    base_str = "Real";
    break;
  case BOOL:
    base_str = "Bool";
    break;
  case STRING:
    base_str = "String";
    break;
  case DATE:
    base_str = "Date";
    break;
  case TIME:
    base_str = "Time";
    break;
  case EVENT:
    base_str = "Event";
    break;
  }

  return std::format("{}{}", base_str, nullable ? "?" : "");
}

std::string node_str(NodeId id, const TypedNodePool &pool) {
  if (id == NODEID_NONE) {
    return "none";
  }
  return pool[id].show(pool);
}

std::string node_vec_str(const std::vector<NodeId> &vec,
                         const TypedNodePool &pool) {
  std::string res = "[";
  for (std::size_t i = 0; i < vec.size(); ++i) {
    if (i > 0)
      res += ", ";
    res += node_str(vec[i], pool);
  }
  res += "]";
  return res;
}

std::string string_vec_str(const std::vector<std::string_view> &vec);

std::string map_str(const std::flat_map<std::string_view, NodeId> &m,
                    const TypedNodePool &pool) {
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

std::string TypedNode::show(const TypedNodePool &pool) const {
  std::string_view tag;
#define X(N, I)                                                                \
  I(std::holds_alternative<N>(data)) { tag = #N; }
#define F(N) X(N, if)
#define R(N) X(N, else if)
  TYPED_NODE_KIND_LIST(F, R)
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
          [&](const Error &n) { return std::format("msg={}", n.msg); },
          [&](const Cast &n) {
            return std::format("child={}", node_str(n.child, pool));
          },
          [&](const Inline &n) {
            return std::format(
                "callee={}, formula={}, params={}", node_str(n.callee, pool),
                node_str(n.formula, pool), node_vec_str(n.params, pool));
          }},
      data);

  return std::format("{}[{}]({})", tag, type.show(), fields);
}
