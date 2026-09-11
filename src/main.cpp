#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "purity.hpp"
#include "sort.hpp"
#include "typing.hpp"
#include <iostream>
#include <print>

int main() {
  std::string input((std::istreambuf_iterator<char>(std::cin)),
                    std::istreambuf_iterator<char>());

  Lexer lexer{input};

  std::println("==Lexer==\n");
  while (lexer.get()) {
    std::println("{}", lexer.get()->show());
    lexer.next();
  }
  std::println("\n==Lexer==");

  Parser parser{{input}};

  auto ast = parser.parse();

  std::println("==Parser==\n");
  for (auto &tl : ast.tls) {
    std::println("{}", ast[tl].show(ast.pool));
  }
  std::println("\n==Parser==");

  auto sorted = topo_sort(ast);
  std::println("TLs={}\nGraph={}\nOrder={}", sorted.tls_names, sorted.deps,
               sorted.order);

  auto impures = impure_fns(ast, sorted.order, sorted.deps);
  std::println("Impures={}", impures);

  std::println("==Typing==\n");
  Typing typing{ast, sorted};
  auto typed = typing.run();

  for (auto tl : typed.tls) {
    std::println("{}", typed[tl].show(typed.pool));
  }
  std::println("\n==Typing==");
}
