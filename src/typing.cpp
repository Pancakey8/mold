#include "typing.hpp"
#include "ast.hpp"
#include "sort.hpp"
#include "utils.hpp"
#include <flat_map>
#include <flat_set>
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
              join(ty_l, ty_r);
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
                  {{MoldType::INT, ty_l.nullable},
                   {MoldType::REAL, ty_r.nullable}},
                  {{MoldType::REAL, ty_l.nullable},
                   {MoldType::INT, ty_r.nullable}},
                  {{MoldType::DATE, ty_l.nullable},
                   {MoldType::DATE, ty_r.nullable}},
                  {{MoldType::TIME, ty_l.nullable},
                   {MoldType::TIME, ty_r.nullable}},
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
                  {{MoldType::INT, ty_l.nullable},
                   {MoldType::REAL, ty_r.nullable},
                   {MoldType::REAL, nullable}},
                  {{MoldType::REAL, ty_l.nullable},
                   {MoldType::INT, ty_r.nullable},
                   {MoldType::REAL, nullable}},
                }
              };
              // clang-format on
              if (n.kind == BinaryOp::ADD) {
                c.cases.push_back({{MoldType::STRING, ty_l.nullable},
                                   {MoldType::STRING, ty_r.nullable},
                                   {MoldType::STRING, nullable}});
                c.cases.push_back({{MoldType::DATE, ty_l.nullable},
                                   {MoldType::TIME, ty_r.nullable},
                                   {MoldType::DATE, nullable}});
                c.cases.push_back({{MoldType::TIME, ty_l.nullable},
                                   {MoldType::DATE, ty_r.nullable},
                                   {MoldType::DATE, nullable}});
              }
              if (n.kind == BinaryOp::SUB) {
                c.cases.push_back({{MoldType::DATE, ty_l.nullable},
                                   {MoldType::TIME, ty_r.nullable},
                                   {MoldType::DATE, nullable}});
                c.cases.push_back({{MoldType::DATE, ty_l.nullable},
                                   {MoldType::DATE, ty_r.nullable},
                                   {MoldType::TIME, nullable}});
              }
              ctrs.push_back(c);
              return push_node(BinaryOp{n.kind, l, r}, ast[id].source, res);
            } break;
            case BinaryOp::COAL: {
              if (!ty_l.nullable) {
                assert(false && "TODO: Error handling");
              }
              auto res = join(ty_l, ty_r);
              return push_node(BinaryOp{n.kind, l, r}, ast[id].source, res);
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
              ty_res = join(ty_tru, ty_fals);
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
            auto callee_id = sorting.tls_names.at(n.name);
            const auto &callee = ast[callee_id];
            if (auto fn = std::get_if<Function>(&callee.data)) {
              if (fn->params.size() != n.params.size())
                assert(false && "TODO: Error handling");
              std::vector<NodeId> params{};
              for (auto p : n.params) {
                params.push_back(infer(p));
              }
              for (std::size_t i = 0; i < params.size(); ++i) {
                locals.push_back({fn->params[i], inferred[params[i].id]});
              }
              auto form = infer(fn->init);
              locals.resize(locals.size() - params.size());
              return push_node(Inline{n.name, std::move(params), form},
                               ast[id].source, inferred[form.id]);
            } else if (sigs.contains(n.name)) {
              const auto &sig = sigs.at(n.name);
              if (sig.params.size() != n.params.size())
                assert(false && "TODO: Error handling");
              std::vector<NodeId> params{};
              for (std::size_t i = 0; i < n.params.size(); ++i) {
                auto p = infer(n.params[i]);
                if (!assignable(inferred[p.id], sig.params[i]))
                  assert(false && "TODO: Error handling");
                params.push_back(p);
              }
              return push_node(FuncCall{n.name, std::move(params)},
                               ast[id].source, sig.ret);
            } else {
              assert(false && "TODO: Error handling");
            }
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
            if (globals.contains(n.name)) {
              if (!unify(globals[n.name], ty_sig))
                assert(false && "TODO: Error handling");
            }
            globals[n.name] = ty_sig;
            return push_node(Input{n.name, sig}, ast[id].source, ty_sig);
          },
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
            auto init = infer(n.init);
            auto ty_init = inferred[init.id];
            if (globals.contains(n.name)) {
              if (!unify(globals[n.name], ty_init))
                assert(false && "TODO: Error handling");
            }
            globals[n.name] = ty_init;
            if (!unify(ty_init, InternType::concrete(MoldType::EVENT, true)))
              assert(false && "TODO: Error handling");
            return push_node(Signal{n.name, init}, ast[id].source, ty_init);
          },
          [&](const Output &n) -> NodeId {
            std::flat_map<std::string_view, NodeId> params{};
            Signature sig{};

            for (auto [pname, pid] : n.params) {
              auto p = infer(pid);
              params[pname] = p;
              sig.params.push_back(inferred[p.id]);
            }

            sig.ret = InternType::concrete(MoldType::EVENT);

            sigs[n.name] = sig;
            return push_node(Output{n.name, std::move(params)}, ast[id].source,
                             sig.ret);
          },
          [&](const Extern &n) -> NodeId {
            std::flat_map<std::string_view, NodeId> params{};
            Signature sig{};

            for (auto [pname, pid] : n.params) {
              auto p = infer(pid);
              params[pname] = p;
              sig.params.push_back(inferred[p.id]);
            }

            auto r = infer(n.ret);

            sig.ret = inferred[r.id];
            sigs[n.name] = sig;

            return push_node(Extern{n.name, std::move(params), r, n.pure},
                             ast[id].source, sig.ret);
          },
          [&](const Function &) -> NodeId {
            // Not individually handled
            return NODEID_NONE;
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

InternType Typing::join(InternType a, InternType b) {
  auto res = InternType::var(fresh(), a.nullable || b.nullable);
#define X(T)                                                                   \
  {{MoldType::T, a.nullable},                                                  \
   {MoldType::T, b.nullable},                                                  \
   {MoldType::T, a.nullable || b.nullable}},
  Constraint c{.vars = {a, b, res}, .cases = {TYPE_BASE_LIST(X)}};
#undef X
  c.cases.push_back({{MoldType::INT, a.nullable},
                     {MoldType::REAL, b.nullable},
                     {MoldType::REAL, a.nullable || b.nullable}});
  c.cases.push_back({{MoldType::REAL, a.nullable},
                     {MoldType::INT, b.nullable},
                     {MoldType::REAL, a.nullable || b.nullable}});
  ctrs.push_back(std::move(c));
  propagate();
  return res;
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

bool Typing::assignable(InternType arg, InternType param) {
  if (unify(arg, param))
    return true;
  auto ab = get_base(arg), pb = get_base(param);
  if (ab && pb && *ab == MoldType::INT && *pb == MoldType::REAL)
    return unify(arg, InternType::concrete(MoldType::INT, arg.nullable));
  return false;
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
          [&](const Inline &n) {
            return std::format("callee={}, formula={}, params={}", n.callee,
                               node_str(n.formula, pool),
                               node_vec_str(n.params, pool));
          }},
      data);

  return std::format("{}[{}]({})", tag, type.show(), fields);
}

bool TypedNode::is_toplevel() const {
  return std::visit(
      [](const auto &node) {
        return Node::is_toplevel_v<std::decay_t<decltype(node)>>;
      },
      data);
}

std::string_view TypedNode::toplevel_name() const {
  return std::visit(
      [](const auto &node) -> std::string_view {
        using T = std::decay_t<decltype(node)>;
        if constexpr (Node::is_toplevel_v<T>) {
          return node.name;
        } else {
          assert(false && "Name of non-top-level requested");
        }
      },
      data);
}

std::flat_map<NodeId, std::vector<NodeId>>
migrate_deps(const std::flat_map<NodeId, std::vector<NodeId>> &untyped_deps,
             const std::flat_map<NodeId, NodeId> &sort_to_ty) {
  std::flat_map<NodeId, std::vector<NodeId>> typed_deps;

  for (const auto &[sid, tid] : sort_to_ty) {
    std::vector<NodeId> targets;
    std::vector<NodeId> stack;
    std::flat_set<NodeId> visited;

    if (auto it = untyped_deps.find(sid); it != untyped_deps.end()) {
      for (auto nbor : it->second) {
        stack.push_back(nbor);
      }
    }

    while (!stack.empty()) {
      auto curr = stack.back();
      stack.pop_back();

      if (!visited.insert(curr).second) {
        continue;
      }

      if (sort_to_ty.contains(curr)) {
        targets.push_back(sort_to_ty.at(curr));
      } else {
        if (auto it = untyped_deps.find(curr); it != untyped_deps.end()) {
          for (auto neighbor : it->second) {
            stack.push_back(neighbor);
          }
        }
      }
    }

    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

    if (!targets.empty()) {
      typed_deps[tid] = std::move(targets);
    }
  }

  return typed_deps;
}

SortResult migrate_sort(const TypedAST &ast, const SortResult &untyped) {
  std::flat_map<NodeId, NodeId> sort_to_ty{};

  for (std::size_t i = 0; i < ast.tls.size(); ++i) {
    if (ast.tls[i] == NODEID_NONE)
      continue;
    sort_to_ty[untyped.order[i]] = ast.tls[i];
  }

  SortResult typed{};
  for (auto [name, sid] : untyped.tls_names) {
    if (sort_to_ty.contains(sid))
      typed.tls_names[name] = sort_to_ty.at(sid);
  }

  typed.order.reserve(untyped.order.size());
  for (auto sid : untyped.order) {
    if (sort_to_ty.contains(sid))
      typed.order.push_back(sort_to_ty.at(sid));
  }

  typed.deps = migrate_deps(untyped.deps, sort_to_ty);

  return typed;
}
