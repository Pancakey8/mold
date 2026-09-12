#include "ast.hpp"
#include "compiler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "purity.hpp"
#include "ranking.hpp"
#include "sort.hpp"
#include "typing.hpp"
#include "utils.hpp"
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
    if (tl != NODEID_NONE)
      std::println("{}", typed[tl].show(typed.pool));
  }
  auto typed_sort = migrate_sort(typed, sorted);
  std::println("TLs={}\nGraph={}\nOrder={}", typed_sort.tls_names,
               typed_sort.deps, typed_sort.order);
  auto ranks = ranks_of(typed_sort);
  std::println("Ranks={}", ranks);
  std::println("\n==Typing==");

  std::println("==Compiler==\n");
  Compiler comp{typed, typed_sort};
  auto prog = comp.run();
  std::println("Consts:");
  for (std::size_t i = 0; i < prog.consts.size(); ++i) {
    auto repr = std::visit(
        overload{[](std::monostate) -> std::string { return "null"; },
                 [](auto &&v) -> std::string { return std::format("{}", v); }},
        prog.consts[i]);
    std::println("  #{} => {}", i, repr);
  }
  std::println("Low IR:");
  for (auto instr : prog.instrs) {
    std::println("{}", instr.show());
  }
  std::println("\n==Compiler==");
}
