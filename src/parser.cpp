#include "parser.hpp"
#include "ast.hpp"
#include "lexer.hpp"
#include <unordered_map>

#define expect(K) \
  if (!lexer.get() || lexer.get()->kind != Token::K) { return pool.push({Error{"Expected " #K}, lexer.get() ? lexer.get()->source : SOURCE_END}); }

NodeId Parser::parse_atom() {
  auto start = lexer.get();

  if (!start) {
    return pool.push({Error{"Expected atom"}, SOURCE_END});
  }

  lexer.next();

  switch (start->kind) {
  case Token::LPAREN: {
    auto ex = parse_expr();
    expect(RPAREN);
    lexer.next();
    return ex;
  } break;
  case Token::KW_LET: {
    expect(IDENT);
    auto name = lexer.get()->data.ident;
    lexer.next();

    expect(DEF_EQ);
    lexer.next();

    auto init = parse_expr();

    expect(KW_IN);
    lexer.next();

    auto body = parse_expr();

    return pool.push({LetIn{name, init, body}, start->source + pool[body].source});
  } break;
  case Token::KW_IF:
    break;
  case Token::KW_PRE:
    break;
  case Token::LIT_BOOL:
    break;
  case Token::LIT_NULL:
    break;
  case Token::IDENT:
    break; // Call, or ident
  case Token::LIT_REAL:
    break;
  case Token::LIT_INT:
    break;
  case Token::LIT_STRING:
    break;
  default:
    return pool.push({Error{"Expected atom"}, start->source});
  }
}

const static std::unordered_map<Token::Kind, int> PREC_TABLE{};

int prec_of(Token::Kind tok) {
  auto p = PREC_TABLE.find(tok);
  return p == PREC_TABLE.end() ? (-1) : p->second;
}

NodeId Parser::parse_led(int prec) {
  auto left = parse_atom();

  while (lexer.get() && prec_of(lexer.get()->kind) >= prec) {
    
  }

  return left;
}

NodeId Parser::parse_type() {}

NodeId Parser::parse_tl() {
  auto start = lexer.get();

  if (!start) {
    return pool.push({Error{"Expected top-level"}, SOURCE_END});
  }

  lexer.next();

  switch (start->kind) {
  case Token::KW_IN:
    break;
  case Token::KW_OUT:
    break;
  case Token::KW_DEF:
    break;
  case Token::KW_SIGNAL:
    break;
  case Token::KW_EXTERN:
    break;
  case Token::KW_FUNCTION:
    break;
  default:
    return pool.push({Error{"Expected top-level"}, SOURCE_END});
  }
}
