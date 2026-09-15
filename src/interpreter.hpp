#pragma once

#include "compiler.hpp"
#include <cstdint>
#include <memory>
#include <queue>
#include <stack>
#include <variant>

namespace mold::internal {

struct InternString {
  std::uint32_t rc;
  std::size_t len;
  char data[];
};

struct InternEvent;

struct InternValue {
  union Data {
    std::int64_t i;
    double r;
    bool b;
    InternString *s;
    InternEvent *e;
    std::int64_t d;
    std::int64_t t;
  } data;

  enum Tag : std::uint8_t {
    NIL,
    INT,
    REAL,
    BOOL,
    STRING,
    EVENT,
    DATE,
    TIME,
  } tag;

  static InternValue of_string(std::string_view s);
  static InternValue of_event(std::uint32_t event_tag,
                              std::span<const InternValue> args);
  const std::string_view as_string() const;
  void inc();
  void dec();
  void destroy();

  bool operator==(const InternValue &other) const;
};

struct InternEvent {
  std::uint32_t rc;
  std::uint32_t tag;
  std::uint16_t argc;
  InternValue args[];
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

struct RingBuffer {
  std::vector<InternValue> buffer;
  std::size_t capacity{0};
  std::size_t head{0};

  void init(std::size_t cap);

  void push(InternValue val);

  InternValue get(std::size_t depth) const;

  ~RingBuffer();
};

struct TickParam {
  std::uint32_t id;
  InternValue value;
};

using ExtFunction =
    std::function<InternValue(std::uint16_t argc, InternValue *argv)>;
using HandlerFunction =
    std::function<void(std::uint16_t argc, InternValue *argv)>;

class Interpreter {
public:
  Interpreter(const Interpreter &) = delete;
  Interpreter(Interpreter &&) = default;
  Interpreter &operator=(const Interpreter &) = delete;
  Interpreter &operator=(Interpreter &&) = delete;

  Interpreter(const Program &prog) : prog(prog) { init(); }

  ~Interpreter();

  void feed(std::uint32_t id, InternValue val) { store(id, val); }

  void implement(std::uint32_t id, ExtFunction fn) { exts[id] = fn; }

  void on(std::uint32_t id, HandlerFunction fn) { listeners[id] = fn; }

  InternValue read(std::uint32_t id) {
    vals[id].inc();
    return vals[id];
  }

  void tick() {
    run();
    dispatch();
    commit();
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
  std::vector<ExtFunction> exts{};
  std::vector<HandlerFunction> listeners{};
  std::vector<std::uint32_t> dispatches{};
  std::vector<RingBuffer> histories{};
  std::vector<InternValue> current{};

  void init();

  void run();

  void cleanup();

  void dispatch();

  void commit();

  void store(std::uint32_t id, InternValue value);
};

} // namespace mold::internal
