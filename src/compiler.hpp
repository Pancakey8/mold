#pragma once

#include "ast.hpp"
#include "sort.hpp"
#include "typing.hpp"
#include <cstdint>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

enum class Op : std::uint8_t {
  // Load constant from table.
  CONST,
  // Load formula value
  LOAD,
  // Store to formula, mark formula as live.
  STORE,
  // Load local value
  LOCAL,
  // Push to local binding
  PUSH_LOCAL,
  // Pop local binding
  POP_LOCAL,
  // Operators
  AND,
  OR,
  BITAND,
  BITOR,
  EQ,
  NEQ,
  LT,
  GT,
  LE,
  GE,
  SHL,
  SHR,
  ADD,
  SUB,
  MULT,
  DIV,
  MOD,
  COAL,
  // Jump true (offset)
  JT,
  // Load formula at depth (offset of tick)
  LOAD_AT,
  // Invoke FFI
  CALL,
  // Construct object of ID (only Events for now)
  CTOR,
  // Multiply, push new `ip` at rank N to deduplicating prio queue
  // of cursors
  MP,
  // Check if a formula is alive or dead
  PULL,
  // Dispatch event
  DISPATCH,
  // Terminate current cursor
  TERM
};

struct Instr {
  union {
    std::uint32_t u;
    std::int32_t i;
  } arg;
  std::uint16_t ext;
  Op kind;
};

#define HINSTR_KIND_LIST(FIRST, REST)                                          \
  FIRST(Upcast)                                                                \
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

struct HInstr {
  using GlobId = std::string_view;
  using LocId = std::uint32_t;
  using LabelId = std::uint32_t;
  using ExtId = std::string_view;
  using EventId = std::string_view;
  using VertId = std::string_view;

  struct Const {
    std::variant<std::int64_t, double, bool, std::string_view, std::monostate>
        val;
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

  struct Comment { std::string message; };

  struct Upcast {};

#define F(T) T
#define R(T) , T
  using Var = std::variant<HINSTR_KIND_LIST(F, R)>;
#undef F
#undef R

  Var data;

  std::string show() const;
};

class Compiler {
public:
  Compiler(const TypedAST &ast, const SortResult &sorting)
      : ast(ast), sorting(sorting) {}

  void run();

private:
  const TypedAST &ast;
  const SortResult &sorting;

  std::vector<HInstr> instrs{};
  std::vector<std::string_view> locals{};
  std::unordered_set<std::string_view> events{};
  std::flat_map<NodeId, std::size_t> ranks{};

  void compile(NodeId id);
  void compile_binop(BinaryOp::Kind id);

  std::uint32_t labels{};
  std::uint32_t fresh_label();
};
