#pragma once

#include "ast.hpp"
#include "lexer.hpp"

namespace mold::internal {

class Parser {
public:
  Parser(Lexer lexer) : lexer(std::move(lexer)) {}

  AST parse();

private:
  Lexer lexer;
  NodePool pool;

  NodeId parse_atom();
  NodeId parse_led(int prec);
  NodeId parse_expr() { return parse_led(0); }
  NodeId parse_type();
  NodeId parse_tl();

  std::pair<std::string_view, NodeId> parse_param();
  std::vector<NodeId> parse_expr_list(Token::Kind close_delim);
  std::vector<std::pair<std::string_view, NodeId>>
  parse_param_list(Token::Kind close_delim);
};

}; // namespace foo::internal
