#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

#define TOKEN_KIND_LIST(X)                                                     \
  X(LIT_INT)                                                                   \
  X(LIT_REAL)                                                                  \
  X(LIT_BOOL)                                                                  \
  X(LIT_STRING)                                                                \
  X(LIT_NULL)                                                                  \
  X(IDENT)                                                                     \
  X(AAND)                                                                      \
  X(OOR)                                                                       \
  X(AND)                                                                       \
  X(OR)                                                                        \
  X(EQ)                                                                        \
  X(NEQ)                                                                       \
  X(LT)                                                                        \
  X(GT)                                                                        \
  X(LE)                                                                        \
  X(GE)                                                                        \
  X(SHL)                                                                       \
  X(SHR)                                                                       \
  X(PLUS)                                                                      \
  X(MINUS)                                                                     \
  X(AST)                                                                       \
  X(SLASH)                                                                     \
  X(PERC)                                                                      \
  X(QUES)                                                                      \
  X(LPAREN)                                                                    \
  X(RPAREN)                                                                    \
  X(KW_LET)                                                                    \
  X(DEF_EQ)                                                                    \
  X(KW_IN)                                                                     \
  X(KW_IF)                                                                     \
  X(KW_THEN)                                                                   \
  X(KW_ELSE)                                                                   \
  X(KW_PRE)                                                                    \
  X(COMMA)                                                                     \
  X(COLON)                                                                     \
  X(KW_OUT)                                                                    \
  X(KW_DEF)                                                                    \
  X(KW_SIGNAL)                                                                 \
  X(KW_EXTERN)                                                                 \
  X(KW_PURE)                                                                   \
  X(KW_IMPURE)                                                                 \
  X(KW_FUNCTION)                                                               \
  X(ERROR)

struct Source {
  std::size_t start, end;

  Source operator+(const Source &other) const {
    return Source{std::min(start, other.start), std::max(end, other.end)};
  }
};

static constexpr Source SOURCE_BEGIN{0, 0};
static constexpr Source SOURCE_END{SIZE_MAX, SIZE_MAX};

struct Token {
  Source source;
  union {
    std::int64_t litInt;
    double litReal;
    bool litBool;
    std::string_view litString;
    std::string_view ident;
    std::string_view error;
  } data;
  enum Kind {
#define X(name) name,
    TOKEN_KIND_LIST(X)
#undef X
  } kind;

  std::string show() const;
};

class Lexer {
public:
  Lexer(std::string_view input) : input(input), cursor{0}, current{} { next(); }

  void next();
  std::optional<Token> get() { return current; }

private:
  std::string_view input;
  std::size_t cursor;
  std::optional<Token> current;

  void skip_ws_comment();
  bool matches(std::string_view prefix);
  char peek(size_t offset = 0);
  char advance();
  bool eof();
};
