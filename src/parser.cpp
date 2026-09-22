#include "mold/parser.hpp"
#include "mold/ast.hpp"
#include "mold/lexer.hpp"
#include <print>

namespace mold::internal {

#define expect(K)                                                              \
  if (!lexer.get() || lexer.get()->kind != Token::K) {                         \
    return pool.push({Error{"Expected " #K},                                   \
                      lexer.get() ? lexer.get()->source : SOURCE_END});        \
  }

AST Parser::parse() {
  std::vector<NodeId> tls;
  while (lexer.get()) {
    tls.push_back(parse_tl());
  }
  return AST{std::move(pool), std::move(tls)};
}

NodeId Parser::parse_atom() {
  auto start = lexer.get();

  if (!start) {
    return pool.push({Error{"Expected atom"}, SOURCE_END});
  }

  switch (start->kind) {
  case Token::LPAREN: {
    lexer.next();
    auto ex = parse_expr();
    expect(RPAREN);
    lexer.next();
    return ex;
  }
  case Token::KW_LET: {
    lexer.next();
    expect(IDENT);
    auto name = lexer.get()->data.ident;
    lexer.next();

    NodeId type{NODEID_NONE};

    if (lexer.get() && lexer.get()->kind == Token::COLON) {
      lexer.next();
      type = parse_type();
    }

    expect(DEF_EQ);
    lexer.next();

    auto init = parse_expr();

    expect(KW_IN);
    lexer.next();

    auto body = parse_expr();

    return pool.push(
        {LetIn{name, type, init, body}, start->source + pool[body].source});
  }
  case Token::KW_IF: {
    lexer.next();
    auto cond = parse_expr();

    expect(KW_THEN);
    lexer.next();
    auto tru = parse_expr();

    NodeId fals = NODEID_NONE;
    Source end_source = pool[tru].source;

    if (lexer.get() && lexer.get()->kind == Token::KW_ELSE) {
      lexer.next();
      fals = parse_expr();
      end_source = pool[fals].source;
    }

    return pool.push({IfElse{cond, tru, fals}, start->source + end_source});
  }
  case Token::KW_PRE: {
    lexer.next();
    expect(LPAREN);
    lexer.next();

    expect(IDENT);
    auto name = lexer.get()->data.ident;
    lexer.next();

    std::uint32_t depth = 1;

    if (lexer.get() && lexer.get()->kind == Token::COMMA) {
      lexer.next();
      expect(LIT_INT);
      auto n = lexer.get()->data.litInt;
      if (!(0 < n && n <= UINT32_MAX)) {
        return pool.push(
            {Error{"Expected positive number"}, lexer.get()->source});
      }
      depth = static_cast<std::uint32_t>(lexer.get()->data.litInt);
      lexer.next();
    }

    expect(RPAREN);
    auto rparen = lexer.get();
    lexer.next();

    return pool.push({PreVal{name, depth}, start->source + rparen->source});
  }
  case Token::LIT_BOOL: {
    bool val = start->data.litBool;
    Source src = start->source;
    lexer.next();
    return pool.push({LitBool{val}, src});
  }
  case Token::LIT_NULL: {
    Source src = start->source;
    lexer.next();
    return pool.push({LitNull{}, src});
  }
  case Token::IDENT: {
    auto name = start->data.ident;
    Source src = start->source;
    lexer.next();

    if (lexer.get() && lexer.get()->kind == Token::LPAREN) {
      lexer.next();
      std::vector<NodeId> params = parse_expr_list(Token::RPAREN);
      expect(RPAREN);
      auto end_src = lexer.get()->source;
      lexer.next();
      return pool.push({FuncCall{name, std::move(params)}, src + end_src});
    } else if (lexer.get() && lexer.get()->kind == Token::LCURLY) {
      lexer.next();
      std::vector<std::pair<std::string_view, NodeId>> inits{};
      while (lexer.get() && lexer.get()->kind != Token::RCURLY) {
        expect(IDENT);
        auto field_name = lexer.get()->data.ident;
        auto field_src = lexer.get()->source;
        lexer.next();

        expect(DEF_EQ);
        lexer.next();

        NodeId expr = parse_expr();

        if (std::find_if(inits.begin(), inits.end(), [&](const auto &p) {
              return p.first == field_name;
            }) != inits.end()) {
          return pool.push({Error{"Duplicate field name"}, field_src});
        }

        inits.push_back({field_name, expr});

        if (lexer.get() && lexer.get()->kind == Token::COMMA) {
          lexer.next();
        } else {
          break;
        }
      }

      expect(RCURLY);
      auto end_src = lexer.get()->source;
      lexer.next();

      return pool.push({StructConst{name, std::move(inits)}, src + end_src});
    }

    return pool.push({Ident{name}, src});
  }
  case Token::LIT_REAL: {
    double val = start->data.litReal;
    Source src = start->source;
    lexer.next();
    return pool.push({LitReal{val}, src});
  }
  case Token::LIT_INT: {
    std::int64_t val = start->data.litInt;
    Source src = start->source;
    lexer.next();
    return pool.push({LitInt{val}, src});
  }
  case Token::LIT_STRING: {
    std::string_view val = start->data.litString;
    Source src = start->source;
    lexer.next();
    return pool.push({LitString{val}, src});
  }
  default:
    lexer.next();
    return pool.push({Error{"Expected atom"}, start->source});
  }
}

int prec_of(Token::Kind tok) {
  switch (tok) {
  case Token::AAND:
  case Token::OOR:
    return 10;
  case Token::AND:
  case Token::OR:
    return 20;
  case Token::EQ:
  case Token::NEQ:
    return 30;
  case Token::LT:
  case Token::GT:
  case Token::LE:
  case Token::GE:
    return 40;
  case Token::SHL:
  case Token::SHR:
    return 50;
  case Token::PLUS:
  case Token::MINUS:
    return 60;
  case Token::AST:
  case Token::SLASH:
  case Token::PERC:
    return 70;
  case Token::QUES:
    return 80;
  case Token::ARROW:
    return 90;
  default:
    return -1;
  }
}

BinaryOp::Kind binop_kind_of(Token::Kind tok) {
  switch (tok) {
  case Token::AAND:
    return BinaryOp::AND;
  case Token::OOR:
    return BinaryOp::OR;
  case Token::AND:
    return BinaryOp::BITAND;
  case Token::OR:
    return BinaryOp::BITOR;
  case Token::EQ:
    return BinaryOp::EQ;
  case Token::NEQ:
    return BinaryOp::NEQ;
  case Token::LT:
    return BinaryOp::LT;
  case Token::GT:
    return BinaryOp::GT;
  case Token::LE:
    return BinaryOp::LE;
  case Token::GE:
    return BinaryOp::GE;
  case Token::SHL:
    return BinaryOp::SHL;
  case Token::SHR:
    return BinaryOp::SHR;
  case Token::PLUS:
    return BinaryOp::ADD;
  case Token::MINUS:
    return BinaryOp::SUB;
  case Token::AST:
    return BinaryOp::MULT;
  case Token::SLASH:
    return BinaryOp::DIV;
  case Token::PERC:
    return BinaryOp::MOD;
  case Token::QUES:
    return BinaryOp::COAL;
  case Token::ARROW:
    return BinaryOp::MEMB;
  default:
    assert(false && "Invalid binary operator token");
  }
}

NodeId Parser::parse_led(int prec) {
  auto left = parse_atom();

  while (lexer.get()) {
    auto tok = lexer.get();
    int p = prec_of(tok->kind);
    if (p < prec) {
      break;
    }

    lexer.next();
    auto right = parse_led(p + 1);
    Source src = pool[left].source + pool[right].source;
    left = pool.push({BinaryOp{binop_kind_of(tok->kind), left, right}, src});
  }

  return left;
}

NodeId Parser::parse_type() {
  expect(IDENT);
  auto base = lexer.get()->data.ident;
  Source src = lexer.get()->source;
  lexer.next();

  bool nullable = false;
  if (lexer.get() && lexer.get()->kind == Token::QUES) {
    src = src + lexer.get()->source;
    lexer.next();
    nullable = true;
  }

  return pool.push({TypeName{base, nullable}, src});
}

std::vector<NodeId> Parser::parse_expr_list(Token::Kind close_delim) {
  std::vector<NodeId> list;
  while (lexer.get() && lexer.get()->kind != close_delim) {
    list.push_back(parse_expr());
    if (lexer.get() && lexer.get()->kind == Token::COMMA) {
      lexer.next();
    } else {
      break;
    }
  }
  return list;
}

std::pair<std::string_view, NodeId> Parser::parse_param() {
  if (!lexer.get() || lexer.get()->kind != Token::IDENT) {
    return {"Error",
            pool.push({Error{"Expected IDENT"},
                       lexer.get() ? lexer.get()->source : SOURCE_END})};
  };
  auto name = lexer.get()->data.ident;
  lexer.next();

  if (!lexer.get() || lexer.get()->kind != Token::COLON) {
    return {name, pool.push({Error{"Expected COLON"},
                             lexer.get() ? lexer.get()->source : SOURCE_END})};
  };
  lexer.next();

  NodeId type = parse_type();
  return {name, type};
}

std::vector<std::pair<std::string_view, NodeId>>
Parser::parse_param_list(Token::Kind close_delim) {
  std::vector<std::pair<std::string_view, NodeId>> params;
  while (lexer.get() && lexer.get()->kind != close_delim) {
    params.push_back(parse_param());
    if (lexer.get() && lexer.get()->kind == Token::COMMA) {
      lexer.next();
    } else {
      break;
    }
  }
  return params;
}

NodeId Parser::parse_tl() {
  auto start = lexer.get();

  if (!start) {
    return pool.push({Error{"Expected top-level"}, SOURCE_END});
  }

  switch (start->kind) {
  case Token::KW_IN: {
    lexer.next();
    expect(IDENT);
    auto name = lexer.get()->data.ident;
    lexer.next();

    expect(COLON);
    lexer.next();

    NodeId type = parse_type();

    return pool.push({Input{name, type}, start->source + pool[type].source});
  }
  case Token::KW_OUT: {
    lexer.next();
    expect(IDENT);
    auto name = lexer.get()->data.ident;
    lexer.next();

    expect(LPAREN);
    lexer.next();

    auto param_pairs = parse_param_list(Token::RPAREN);

    expect(RPAREN);
    auto end_src = lexer.get()->source;
    lexer.next();

    std::vector<std::pair<std::string_view, NodeId>> params;
    for (auto &[pname, ptype] : param_pairs) {
      if (std::find_if(params.begin(), params.end(), [&pname](auto p) {
            return p.first == pname;
          }) != params.end())
        return pool.push(
            {Error{"Duplicate parameter name"}, pool[ptype].source});
      params.push_back({pname, ptype});
    }

    return pool.push(
        {Output{name, std::move(params)}, start->source + end_src});
  }
  case Token::KW_DEF: {
    lexer.next();
    expect(IDENT);
    auto name = lexer.get()->data.ident;
    lexer.next();

    NodeId type{NODEID_NONE};
    if (lexer.get() && lexer.get()->kind == Token::COLON) {
      lexer.next();
      type = parse_type();
    }

    expect(DEF_EQ);
    lexer.next();

    NodeId init = parse_expr();

    return pool.push(
        {Formula{name, type, init}, start->source + pool[init].source});
  }
  case Token::KW_SIGNAL: {
    lexer.next();
    expect(IDENT);
    auto name = lexer.get()->data.ident;
    lexer.next();

    expect(DEF_EQ);
    lexer.next();

    NodeId init = parse_expr();

    return pool.push({Signal{name, init}, start->source + pool[init].source});
  }
  case Token::KW_EXTERN: {
    lexer.next();
    bool pure = true;
    if (lexer.get() && lexer.get()->kind == Token::KW_PURE) {
      lexer.next();
    } else if (lexer.get() && lexer.get()->kind == Token::KW_IMPURE) {
      pure = false;
      lexer.next();
    } else {
      return pool.push({Error{"Expected 'pure' or 'impure'"}, start->source});
    }

    expect(IDENT);
    auto name = lexer.get()->data.ident;
    lexer.next();

    expect(LPAREN);
    lexer.next();

    auto param_pairs = parse_param_list(Token::RPAREN);

    expect(RPAREN);
    lexer.next();

    expect(COLON);
    lexer.next();

    NodeId ret = parse_type();
    Source src = start->source + pool[ret].source;

    std::vector<std::pair<std::string_view, NodeId>> params;
    for (auto &[pname, ptype] : param_pairs) {
      if (std::find_if(params.begin(), params.end(), [&pname](auto p) {
            return p.first == pname;
          }) != params.end())
        return pool.push(
            {Error{"Duplicate parameter name"}, pool[ptype].source});
      params.push_back({pname, ptype});
    }

    return pool.push({Extern{name, std::move(params), ret, pure}, src});
  }
  case Token::KW_FUNCTION: {
    lexer.next();
    expect(IDENT);
    auto name = lexer.get()->data.ident;
    lexer.next();

    expect(LPAREN);
    lexer.next();

    std::vector<std::string_view> params;
    while (lexer.get() && lexer.get()->kind != Token::RPAREN) {
      expect(IDENT);
      params.push_back(lexer.get()->data.ident);
      lexer.next();
      if (lexer.get() && lexer.get()->kind == Token::COMMA) {
        lexer.next();
      } else {
        break;
      }
    }

    expect(RPAREN);
    lexer.next();

    expect(DEF_EQ);
    lexer.next();

    NodeId init = parse_expr();

    return pool.push({Function{name, std::move(params), init},
                      start->source + pool[init].source});
  }
  case Token::KW_STRUCT: {
    lexer.next();
    expect(IDENT);
    auto name = lexer.get()->data.ident;
    lexer.next();

    expect(LCURLY);
    lexer.next();

    auto fields = parse_param_list(Token::RCURLY);

    expect(RCURLY);
    auto end_src = lexer.get()->source;
    lexer.next();

    for (auto &[fname, ftype] : fields) {
      if (std::count_if(fields.begin(), fields.end(),
                        [&fname](auto f) { return f.first == fname; }) != 1) {
        return pool.push({Error{"Duplicate field name"}, pool[ftype].source});
      }
    }

    // std::println("Parse struct {}", name);

    return pool.push(
        {StructDef{name, std::move(fields)}, start->source + end_src});
  }
  default:
    lexer.next();
    return pool.push({Error{"Expected top-level"}, start->source});
  }
}

}; // namespace mold::internal
