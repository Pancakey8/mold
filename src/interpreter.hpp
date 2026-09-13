#pragma once

#include "compiler.hpp"
#include <cstdint>
#include <memory>
#include <stack>
#include <variant>

struct InternString {
  std::uint32_t rc;
  std::size_t len;
  char data[];
};

struct InternValue {
  union Data {
    std::int64_t i;
    double r;
    bool b;
    InternString *s;
  } data;

  enum Tag : std::uint8_t {
    NIL,
    INT,
    REAL,
    BOOL,
    STRING,
    // TODO: DATE, TIME, EVENT
  } tag;

  static InternValue of_string(std::string_view s);
  const std::string_view as_string() const;
  void inc();
  void dec();
  void destroy();

  bool operator==(const InternValue& other) const;
};

struct IPQueue {
public:
  using Rank = std::uint16_t;

  IPQueue() {}

  void push(Rank r, std::size_t ip);
  std::pair<Rank, std::size_t> front();
  void pop();
  bool empty();

private:
  std::vector<std::pair<Rank, std::size_t>> ptrs;
  Rank min{static_cast<Rank>(-1)};
};

struct TickParam {
  std::string_view name;
  InternValue value;
};

class Interpreter {
public:
  Interpreter(const Interpreter &) = delete;
  Interpreter(Interpreter &&) = delete;
  Interpreter &operator=(const Interpreter &) = delete;
  Interpreter &operator=(Interpreter &&) = delete;

  Interpreter(const Program &prog) : prog(prog) { init(); }

  ~Interpreter();

  template <typename... Args> void tick(Args &&...args) {
    (set_input(args.name, args.value), ...);
    run();
    cleanup();
  }

private:
  const Program &prog;
  std::vector<InternValue> consts{};
  IPQueue ip_queue{};
  std::vector<InternValue> vals{};
  std::vector<bool> dirty{};
  std::size_t ip{};
  std::stack<InternValue> stack{};
  std::vector<InternValue> locals{};

  void init();

  void run();

  void cleanup();

  void set_input(std::string_view name, InternValue value);
};
