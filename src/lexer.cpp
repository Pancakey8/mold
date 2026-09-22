#include "mold/lexer.hpp"
#include <charconv>
#include <string>

namespace mold::internal {

bool Lexer::eof() { return cursor >= input.size(); }

char Lexer::peek(size_t offset) {
  if (cursor + offset >= input.size())
    return '\0';
  return input[cursor + offset];
}

char Lexer::advance() {
  if (eof())
    return '\0';
  return input[cursor++];
}

bool Lexer::matches(std::string_view prefix) {
  if (cursor + prefix.size() > input.size())
    return false;
  return input.substr(cursor, prefix.size()) == prefix;
}

bool is_alpha(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

bool is_num(char c) { return ('0' <= c && c <= '9'); }

bool is_alnum(char c) { return is_alpha(c) || is_num(c); }

bool is_space(char c) {
  return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

bool is_eol(char c) { return c == '\r' || c == '\n'; }

void Lexer::next() {
  skip_ws_comment();

  if (eof()) {
    current = std::nullopt;
    return;
  }

  size_t start = cursor;
  char c = advance();

  if (is_alpha(c) || c == '_') {
    bool can_dot{true};
    while (!eof() && (is_alnum(peek()) || peek() == '_' || peek() == '.')) {
      if (peek() == '.') {
        if (!can_dot) break;
        can_dot = false;
      } else {
        can_dot = true;
      }
      advance();
    }
    std::string_view value = input.substr(start, cursor - start);
    Source src{start, cursor};

    Token::Kind kind;
    if (value == "true" || value == "false") {
      kind = Token::LIT_BOOL;
    } else if (value == "null") {
      kind = Token::LIT_NULL;
    } else if (value == "let") {
      kind = Token::KW_LET;
    } else if (value == "in") {
      kind = Token::KW_IN;
    } else if (value == "if") {
      kind = Token::KW_IF;
    } else if (value == "then") {
      kind = Token::KW_THEN;
    } else if (value == "else") {
      kind = Token::KW_ELSE;
    } else if (value == "pre") {
      kind = Token::KW_PRE;
    } else if (value == "out") {
      kind = Token::KW_OUT;
    } else if (value == "def") {
      kind = Token::KW_DEF;
    } else if (value == "signal") {
      kind = Token::KW_SIGNAL;
    } else if (value == "extern") {
      kind = Token::KW_EXTERN;
    } else if (value == "pure") {
      kind = Token::KW_PURE;
    } else if (value == "impure") {
      kind = Token::KW_IMPURE;
    } else if (value == "function") {
      kind = Token::KW_FUNCTION;
    } else if (value == "struct") {
      kind = Token::KW_STRUCT;
    } else {
      kind = Token::IDENT;
    }

    Token tok{src, {}, kind};
    if (kind == Token::LIT_BOOL) {
      tok.data.litBool = (value == "true");
    } else if (kind == Token::IDENT) {
      tok.data.ident = value;
    }

    current = tok;
    return;
  }

  if (is_num(c) || (c == '-' && is_num(peek())) || (c == '+' && is_num(peek()))) {
    bool is_real = false;
    while (!eof() && is_num(peek())) {
      advance();
    }

    if (peek() == '.' && is_num(peek(1))) {
      is_real = true;
      advance();
      while (!eof() && is_num(peek())) {
        advance();
      }
    }

    std::string_view value = input.substr(start, cursor - start);
    Source src{start, cursor};

    Token tok{src, {}, is_real ? Token::LIT_REAL : Token::LIT_INT};
    auto [ptr, ec] =
        is_real ? std::from_chars(value.data(), value.data() + value.size(),
                                  tok.data.litReal)
                : std::from_chars(value.data(), value.data() + value.size(),
                                  tok.data.litInt);

    if (ec != std::errc()) {
      current =
          Token{src, {.error = "Numeric literal too large"}, Token::ERROR};
    } else {
      current = tok;
    }
    return;
  }

  if (c == '"') {
    size_t content_start = cursor;
    while (!eof() && peek() != '"' && !is_eol(peek())) {
      advance();
    }
    size_t content_end = cursor;
    if (peek() == '"') {
      advance();
      Source src{start, cursor};
      std::string_view content =
          input.substr(content_start, content_end - content_start);
      current = Token{src, {.litString = content}, Token::LIT_STRING};
      return;
    }
    Source src{start, cursor};
    current = Token{src, {.error = "Expected closing quote"}, Token::ERROR};
    return;
  }

  if (c == '&' && peek() == '&') {
    advance();
    current = Token{Source{start, cursor}, {}, Token::AAND};
    return;
  }
  if (c == '|' && peek() == '|') {
    advance();
    current = Token{Source{start, cursor}, {}, Token::OOR};
    return;
  }
  if (c == ':' && peek() == '=') {
    advance();
    current = Token{Source{start, cursor}, {}, Token::DEF_EQ};
    return;
  }
  if (c == '<' && peek() == '=') {
    advance();
    current = Token{Source{start, cursor}, {}, Token::LE};
    return;
  }
  if (c == '>' && peek() == '=') {
    advance();
    current = Token{Source{start, cursor}, {}, Token::GE};
    return;
  }
  if (c == '/' && peek() == '=') {
    advance();
    current = Token{Source{start, cursor}, {}, Token::NEQ};
    return;
  }
  if (c == '<' && peek() == '<') {
    advance();
    current = Token{Source{start, cursor}, {}, Token::SHL};
    return;
  }
  if (c == '>' && peek() == '>') {
    advance();
    current = Token{Source{start, cursor}, {}, Token::SHR};
    return;
  }
  if (c == '-' && peek() == '>') {
    advance();
    current = Token{Source{start, cursor}, {}, Token::ARROW};
    return;
  }

  Token::Kind kind;
  switch (c) {
  case '&':
    kind = Token::AND;
    break;
  case '|':
    kind = Token::OR;
    break;
  case '=':
    kind = Token::EQ;
    break;
  case '<':
    kind = Token::LT;
    break;
  case '>':
    kind = Token::GT;
    break;
  case '+':
    kind = Token::PLUS;
    break;
  case '-':
    kind = Token::MINUS;
    break;
  case '*':
    kind = Token::AST;
    break;
  case '/':
    kind = Token::SLASH;
    break;
  case '%':
    kind = Token::PERC;
    break;
  case '?':
    kind = Token::QUES;
    break;
  case '(':
    kind = Token::LPAREN;
    break;
  case ')':
    kind = Token::RPAREN;
    break;
  case '{':
    kind = Token::LCURLY;
    break;
  case '}':
    kind = Token::RCURLY;
    break;
  case ',':
    kind = Token::COMMA;
    break;
  case ':':
    kind = Token::COLON;
    break;
  default:
    current = Token{
        Source{start, cursor}, {.error = "Unknown character"}, Token::ERROR};
    return;
  }

  current = Token{Source{start, cursor}, {}, kind};
}

void Lexer::skip_ws_comment() {
  while (!eof()) {
    char c = peek();
    if (is_space(c)) {
      advance();
    } else if (c == '#') {
      while (!eof() && !is_eol(peek())) {
        advance();
      }
    } else {
      break;
    }
  }
}

std::string Token::show() const {
  std::string result{};
  switch (kind) {
#define X(name)                                                                \
  case name:                                                                   \
    result = #name;                                                            \
    break;
    TOKEN_KIND_LIST(X)
#undef X
  }

  result += "(";
  switch (kind) {
  case LIT_INT:
    result += std::to_string(data.litInt);
    break;
  case LIT_REAL:
    result += std::to_string(data.litReal);
    break;
  case LIT_BOOL:
    result += (data.litBool ? "true" : "false");
    break;
  case LIT_STRING:
    result += "\"" + std::string(data.litString) + "\"";
    break;
  case IDENT:
    result += std::string(data.ident);
    break;
  case ERROR:
    result += std::string(data.error);
    break;
  default:
    break;
  }
  result += ")";

  return result;
}

}; // namespace foo::internal
