#pragma once

#include "ast.hpp"
#include "mold/ranking.hpp"
#include "sort.hpp"
#include "typing.hpp"
#include <cstdint>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

namespace mold::internal {

#define OP_LIST(X)                                                             \
  X(UPCAST)                                                                    \
  X(MKSTR)                                                                     \
  X(MEMB)                                                                      \
  X(TERM)                                                                      \
  X(DISPATCH)                                                                  \
  X(PULL)                                                                      \
  X(MP)                                                                        \
  X(CTOR)                                                                      \
  X(CALL)                                                                      \
  X(LOAD_AT)                                                                   \
  X(JUMP)                                                                      \
  X(JUMP_TRUE)                                                                 \
  X(JUMP_FALSE)                                                                \
  X(COAL)                                                                      \
  X(MOD)                                                                       \
  X(DIV)                                                                       \
  X(MULT)                                                                      \
  X(SUB)                                                                       \
  X(ADD)                                                                       \
  X(SHR)                                                                       \
  X(SHL)                                                                       \
  X(GE)                                                                        \
  X(LE)                                                                        \
  X(GT)                                                                        \
  X(LT)                                                                        \
  X(NEQ)                                                                       \
  X(EQ)                                                                        \
  X(BIT_OR)                                                                    \
  X(BIT_AND)                                                                   \
  X(OR)                                                                        \
  X(AND)                                                                       \
  X(POP_LOCAL)                                                                 \
  X(PUSH_LOCAL)                                                                \
  X(LOCAL)                                                                     \
  X(STORE)                                                                     \
  X(LOAD)                                                                      \
  X(CONST)

enum class Op : std::uint8_t {
#define X(T) T,
  OP_LIST(X)
#undef X
};

struct Instr {
  union {
    std::uint32_t u;
    std::int32_t i;
  } arg;
  std::uint16_t ext;
  Op kind;

  Instr(Op op) : arg{}, ext{}, kind{op} {}
  Instr(Op op, std::int32_t arg) : arg{.i = arg}, ext{}, kind{op} {}
  Instr(Op op, std::uint32_t arg) : arg{.u = arg}, ext{}, kind{op} {}
  Instr(Op op, std::int32_t arg, std::uint16_t ext)
      : arg{.i = arg}, ext{ext}, kind{op} {}
  Instr(Op op, std::uint32_t arg, std::uint16_t ext)
      : arg{.u = arg}, ext{ext}, kind{op} {}

  std::string show() const;
};

#define HINSTR_KIND_LIST(FIRST, REST)                                          \
  FIRST(Upcast)                                                                \
  REST(MkStr)                                                                  \
  REST(Memb)                                                                   \
  REST(Comment)                                                                \
  REST(Term)                                                                   \
  REST(Dispatch)                                                               \
  REST(Pull)                                                                   \
  REST(VertLabel)                                                              \
  REST(Mp)                                                                     \
  REST(Ctor)                                                                   \
  REST(Call)                                                                   \
  REST(LoadAt)                                                                 \
  REST(Label)                                                                  \
  REST(Jump)                                                                   \
  REST(JumpTrue)                                                               \
  REST(JumpFalse)                                                              \
  REST(Coal)                                                                   \
  REST(Mod)                                                                    \
  REST(Div)                                                                    \
  REST(Mult)                                                                   \
  REST(Sub)                                                                    \
  REST(Add)                                                                    \
  REST(Shr)                                                                    \
  REST(Shl)                                                                    \
  REST(Ge)                                                                     \
  REST(Le)                                                                     \
  REST(Gt)                                                                     \
  REST(Lt)                                                                     \
  REST(Neq)                                                                    \
  REST(Eq)                                                                     \
  REST(BitOr)                                                                  \
  REST(BitAnd)                                                                 \
  REST(Or)                                                                     \
  REST(And)                                                                    \
  REST(PopLocal)                                                               \
  REST(PushLocal)                                                              \
  REST(Local)                                                                  \
  REST(Store)                                                                  \
  REST(Load)                                                                   \
  REST(Const)

using ConstVal =
    std::variant<std::int64_t, double, bool, std::string_view, std::monostate>;

struct HInstr {
  using GlobId = std::string_view;
  using LocId = std::uint32_t;
  using LabelId = std::uint32_t;
  using ExtId = std::string_view;
  using EventId = std::string_view;
  using VertId = std::string_view;
  using StructId = std::string_view;
  using FieldId = std::uint32_t;

  struct Const {
    ConstVal val;
  };

  struct Load {
    GlobId glob;
  };

  struct Store {
    GlobId glob;
  };

  struct Local {
    LocId loc;
  };

  struct PushLocal {};

  struct PopLocal {};

  struct And {};

  struct Or {};

  struct BitAnd {};

  struct BitOr {};

  struct Eq {};

  struct Neq {};

  struct Lt {};

  struct Gt {};

  struct Le {};

  struct Ge {};

  struct Shl {};

  struct Shr {};

  struct Add {};

  struct Sub {};

  struct Mult {};

  struct Div {};

  struct Mod {};

  struct Coal {};

  struct JumpFalse {
    LabelId label;
  };

  struct JumpTrue {
    LabelId label;
  };

  struct Jump {
    LabelId label;
  };

  struct Label {
    LabelId label;
  };

  struct LoadAt {
    GlobId glob;
    std::uint16_t depth;
  };

  struct Call {
    ExtId fn;
    std::uint16_t argc;
  };

  struct Ctor {
    EventId fn;
    std::uint16_t argc;
  };

  struct Mp {
    VertId vert;
    std::uint16_t rank;
  };

  struct VertLabel {
    VertId vert;
  };

  struct Pull {
    GlobId glob;
  };

  struct Dispatch {
    GlobId glob;
  };

  struct Term {};

  struct Comment {
    std::string message;
  };

  struct Memb {
    FieldId field;
  };

  struct MkStr {
    StructId str;
    std::uint16_t initc;
  };

  struct Upcast {};

#define F(T) T
#define R(T) , T
  using Var = std::variant<HINSTR_KIND_LIST(F, R)>;
#undef F
#undef R

  Var data;

  std::string show() const;
};

struct Program {
  std::vector<ConstVal> consts;
  std::vector<Instr> instrs;
  std::uint32_t var_count;
  std::uint32_t ext_count;
  std::uint32_t event_count;
};

struct SymbolTable {
  std::vector<HInstr::GlobId> globs{};
  std::vector<HInstr::VertId> verts{};
  std::vector<HInstr::EventId> events{};
  std::vector<HInstr::ExtId> exts{};
  std::vector<HInstr::StructId> structs{};
};

class Compiler {
public:
  Compiler(const TypedAST &ast) : ast(ast) {}

  std::vector<HInstr> run();

private:
  const TypedAST &ast;

  std::vector<HInstr> instrs{};
  std::vector<std::string_view> locals{};
  std::unordered_set<std::string_view> events{};
  Ranking ranking{};

  void compile(NodeId id);
  void compile_binop(BinaryOp::Kind id);

  std::uint32_t labels{};
  std::uint32_t fresh_label();
};

struct HighToLow {
public:
  HighToLow(std::span<const HInstr> hi) : hi(hi) {}

  std::pair<Program, SymbolTable> run();

private:
  std::span<const HInstr> hi;
  std::vector<Instr> lo{};
  SymbolTable syms{};

  void lower(const HInstr &instr);

  std::uint32_t glob_at(HInstr::GlobId id);
  std::uint32_t vert_at(HInstr::VertId id);
  std::uint32_t event_at(HInstr::EventId id);
  std::uint32_t ext_at(HInstr::ExtId id);
  std::uint32_t struct_at(HInstr::StructId id);

  std::vector<std::uint32_t> vert_fixups{};
  std::flat_map<std::uint32_t, std::uint32_t> vert_labels{};

  std::vector<std::uint32_t> label_fixups{};
  std::flat_map<std::uint32_t, std::uint32_t> labels{};

  std::vector<ConstVal> consts{};
};

} // namespace mold::internal
