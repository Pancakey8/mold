#pragma once

#include "ast.hpp"
#include "lexer.hpp"

class Parser {
public:
  Parser(Lexer lexer) : lexer(std::move(lexer)) {}

private:
  Lexer lexer;
  NodePool pool;

  NodeId parse_atom();
  NodeId parse_led(int prec);
  NodeId parse_expr() { return parse_led(0); }
  NodeId parse_type();
  NodeId parse_tl();
};
