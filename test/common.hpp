#pragma once

#include "mold/script.hpp"
#include <cassert>
#include <print>
#include <string_view>

inline mold::Script make_script(std::string_view input) {
  auto res = mold::Script::of_string(input);
  if (!res.has_value()) {
    for (auto &diag : res.error()) {
      std::println("{}", diag.format(input));
    }
    assert(false && "Compilation failed");
  }

  return std::move(*res);
}

[[nodiscard]]
inline bool fail_compile(std::string_view input) {
  auto res = mold::Script::of_string(input);
  if (!res.has_value()) {
    std::println("Expected failures, these are fine:");
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

      std::println(stderr, "{}:{}: {}", line, col, diag.message);
      std::println(stderr, "At {}\n", source_str);
    }
    return true;
  }

  return false;
}
