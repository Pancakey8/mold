#include "ast.hpp"
#include "compiler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "ranking.hpp"
#include "script.hpp"
#include "sort.hpp"
#include "typing.hpp"
#include "utils.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <print>
#include <ranges>
#include <thread>

int main() {
  std::string input((std::istreambuf_iterator<char>(std::cin)),
                    std::istreambuf_iterator<char>());

  auto res = mold::Script::of_string(input);

  if (!res.has_value()) {
    for (auto &diag : res.error()) {
      std::size_t line = 1;
      std::size_t col = 1;

      for (std::size_t i = 0; i < diag.src.start && i < input.size(); ++i) {
        if (input[i] == '\n') {
          line++;
          col = 1;
        } else {
          col++;
        }
      }

      auto start = std::clamp(diag.src.start, 0UZ, input.size() - 1);
      auto end = std::clamp(diag.src.end, 0UZ, input.size() - 1);
      std::string_view source_str =
          std::string_view(input).substr(start, end - start);

      std::println("{}:{}: {}", line, col, diag.message);
      std::println("At {}\n", source_str);
    }

    return 1;
  }

  auto &script = *res;

  script.on("alert", [](auto args) { std::println("(!) ALERT: {}", args[0]); });
  script.implement("now", [](auto) -> mold::Value {
    return {std::chrono::system_clock::now()};
  });
  script.implement("seconds", [](auto args) -> mold::Value {
    auto n = std::get<std::int64_t>(args[0].data);
    return {std::chrono::seconds{n}};
  });
  script.implement("trace", [](auto args) -> mold::Value {
    std::println("{}", args[0]);
    return args[1];
  });

  while (true) {
    script.tick();
    std::println("{} {} {}", script.read("y"), script.read("should_alert"),
                 script.read("foo"));
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }

  // using namespace mold::internal;

  // Lexer lexer{input};

  // std::println("==Lexer==\n");
  // while (lexer.get()) {
  //   std::println("{}", lexer.get()->show());
  //   lexer.next();
  // }
  // std::println("\n==Lexer==");

  // Parser parser{{input}};

  // auto ast = parser.parse();

  // std::println("==Parser==\n");
  // for (auto &tl : ast.tls) {
  //   std::println("{}", ast[tl].show(ast.pool));
  // }
  // std::println("\n==Parser==");

  // auto sorted = topo_sort(ast);
  // std::println("TLs={}\nGraph={}\nOrder={}", sorted.tls_names, sorted.deps,
  //              sorted.order);

  // std::println("==Typing==\n");
  // Typing typing{ast, sorted};
  // auto typed = typing.run();

  // for (auto tl : typed.tls) {
  //   if (tl != NODEID_NONE)
  //     std::println("{}", typed[tl].show(typed.pool));
  // }
  // auto typed_sort = migrate_sort(typed, sorted);
  // std::println("TLs={}\nGraph={}\nOrder={}", typed_sort.tls_names,
  //              typed_sort.deps, typed_sort.order);
  // auto ranks = ranks_of(typed_sort);
  // std::println("Ranks={}", ranks);
  // std::println("\n==Typing==");

  // std::println("==Compiler==\n");
  // Compiler comp{typed, typed_sort};
  // auto hir = comp.run();
  // std::println("High IR:");
  // for (const auto &ir : hir) {
  //   std::println("{}", ir.show());
  // }
  // HighToLow low{hir};
  // auto [prog, syms] = low.run();
  // std::println("Consts:");
  // for (std::size_t i = 0; i < prog.consts.size(); ++i) {
  //   auto repr = std::visit(
  //       overload{[](std::monostate) -> std::string { return "null"; },
  //                [](auto &&v) -> std::string { return std::format("{}", v);
  //                }},
  //       prog.consts[i]);
  //   std::println("  #{} => {}", i, repr);
  // }
  // std::println("Low IR:");
  // for (auto instr : prog.instrs) {
  //   std::println("{}", instr.show());
  // }
  // std::println("\n==Compiler==");
}
