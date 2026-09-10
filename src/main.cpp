#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "sort.hpp"
#include <iostream>
#include <print>

int main() {
  std::string input((std::istreambuf_iterator<char>(std::cin)),
                    std::istreambuf_iterator<char>());

  Lexer lexer{input};

  while (lexer.get()) {
    std::println("{}", lexer.get()->show());
    lexer.next();
  }

  Parser parser{{input}};

  auto ast = parser.parse();

  for (auto &tl : ast.tls) {
    std::println("{}", ast[tl].show(ast.pool));
  }
}
