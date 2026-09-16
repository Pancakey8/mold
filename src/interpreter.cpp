#include "mold/interpreter.hpp"
#include "mold/utils.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <variant>

namespace mold::internal {

std::vector<std::uint16_t> depths_of(const Program &prog);

void Interpreter::init() {
  vals.resize(prog.var_count);
  dirty.resize(prog.var_count);
  current.resize(prog.var_count);
  consts.reserve(prog.consts.size());
  exts.resize(prog.ext_count);
  listeners.resize(prog.event_count);
  for (const auto &c : prog.consts) {
    auto v = std::visit(
        overload{
            [](std::int64_t n) {
              return InternValue{{.i = n}, InternValue::INT};
            },
            [](double n) { return InternValue{{.r = n}, InternValue::REAL}; },
            [](bool n) { return InternValue{{.b = n}, InternValue::BOOL}; },
            [](std::string_view n) { return InternValue::of_string(n); },
            [](std::monostate) { return InternValue{{}, InternValue::NIL}; },
        },
        c);
    consts.push_back(v);
  }
  auto depths = depths_of(prog);
  histories.resize(prog.var_count);
  for (std::size_t i = 0; i < prog.var_count; ++i) {
    if (depths[i] > 0) {
      histories[i].init(depths[i]);
    }
  }
}

void Interpreter::store(std::uint32_t id, InternValue value) {
  if (vals[id] != value) {
    dirty[id] = true;
    current[id] = value;
  }
}

void Interpreter::run() {
  while (true) {
    const auto &instr = prog.instrs[ip];
    switch (instr.kind) {
    case Op::UPCAST: {
      auto v = stack.top();
      if (v.tag == InternValue::INT) {
        stack.pop();
        stack.push(InternValue{{.r = static_cast<double>(v.data.i)},
                               InternValue::REAL});
        v.dec();
      }
      ip++;
    } break;
    case Op::TERM: {
      if (ip_queue.empty()) {
        goto exit;
      }
      auto [r, new_ip] = ip_queue.front();
      ip_queue.pop();
      ip = new_ip;
    } break;
    case Op::PULL: {
      auto id = instr.arg.u;
      bool d = dirty[id];
      if (!d && histories[id].capacity > 0) {
        d = histories[id].get(0) != vals[id];
      }
      stack.push(InternValue{{.b = d}, InternValue::BOOL});
      ip++;
    } break;
    case Op::MP: {
      ip_queue.push(instr.ext, instr.arg.u);
      ip++;
    } break;
    case Op::JUMP: {
      ip += instr.arg.i;
    } break;
    case Op::JUMP_TRUE: {
      auto v = stack.top();
      stack.pop();
      if (v.tag == InternValue::BOOL && v.data.b) {
        ip += instr.arg.i;
      } else {
        ip++;
      }
      v.dec();
    } break;
    case Op::JUMP_FALSE: {
      auto v = stack.top();
      stack.pop();
      if (v.tag != InternValue::BOOL || !v.data.b) {
        ip += instr.arg.i;
      } else {
        ip++;
      }
      v.dec();
    } break;
    case Op::COAL: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      if (l.tag == InternValue::NIL) {
        stack.pop();
        stack.push(r);
        l.dec();
      } else {
        r.dec();
      }
      ip++;
    } break;
    case Op::ADD: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (l.tag == InternValue::NIL || r.tag == InternValue::NIL) {
        stack.push(InternValue{{}, InternValue::NIL});
      } else if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        stack.push(InternValue{{.i = l.data.i + r.data.i}, InternValue::INT});
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        stack.push(InternValue{{.r = l.data.r + r.data.r}, InternValue::REAL});
      } else if (l.tag == InternValue::STRING && r.tag == InternValue::STRING) {
        // TODO: This does two copies
        std::string n{l.as_string()};
        n += r.as_string();
        stack.push(InternValue::of_string(n));
      } else if (l.tag == InternValue::DATE && r.tag == InternValue::TIME) {
        stack.push(InternValue{{.d = l.data.d + r.data.t}, InternValue::DATE});
      } else if (l.tag == InternValue::TIME && r.tag == InternValue::DATE) {
        stack.push(InternValue{{.d = l.data.t + r.data.d}, InternValue::DATE});
      } else if (l.tag == InternValue::TIME && r.tag == InternValue::TIME) {
        stack.push(InternValue{{.t = l.data.t + r.data.t}, InternValue::TIME});
      } else {
        has_error = true;
        error = "Type error in addition";
        l.dec();
        r.dec();
        goto exit;
      }
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::EQ: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      stack.push(InternValue{{.b = l == r}, InternValue::BOOL});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::POP_LOCAL: {
      locals.back().dec();
      locals.pop_back();
      ip++;
    } break;
    case Op::PUSH_LOCAL: {
      auto v = stack.top();
      stack.pop();
      locals.push_back(v);
      ip++;
    } break;
    case Op::LOCAL: {
      auto l = locals[instr.arg.u];
      l.inc();
      stack.push(l);
      ip++;
    } break;
    case Op::STORE: {
      auto v = stack.top();
      stack.pop();
      store(instr.arg.u, v);
      ip++;
    } break;
    case Op::LOAD: {
      InternValue v;
      if (dirty[instr.arg.u]) {
        v = current[instr.arg.u];
      } else {
        v = vals[instr.arg.u];
      }
      v.inc();
      stack.push(v);
      ip++;
    } break;
    case Op::CONST: {
      auto v = consts[instr.arg.u];
      v.inc();
      stack.push(v);
      ip++;
    } break;
    case Op::SUB: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (l.tag == InternValue::NIL || r.tag == InternValue::NIL) {
        stack.push(InternValue{{}, InternValue::NIL});
      } else if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        stack.push(InternValue{{.i = l.data.i - r.data.i}, InternValue::INT});
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        stack.push(InternValue{{.r = l.data.r - r.data.r}, InternValue::REAL});
      } else if (l.tag == InternValue::DATE && r.tag == InternValue::TIME) {
        stack.push(InternValue{{.d = l.data.d - r.data.t}, InternValue::DATE});
      } else if (l.tag == InternValue::DATE && r.tag == InternValue::DATE) {
        stack.push(InternValue{{.t = l.data.d - r.data.d}, InternValue::TIME});
      } else if (l.tag == InternValue::TIME && r.tag == InternValue::TIME) {
        stack.push(InternValue{{.t = l.data.t - r.data.t}, InternValue::TIME});
      } else {
        has_error = true;
        error = "Type error in subtraction";
        l.dec();
        r.dec();
        goto exit;
      }
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::MULT: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (l.tag == InternValue::NIL || r.tag == InternValue::NIL) {
        stack.push(InternValue{{}, InternValue::NIL});
      } else if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        stack.push(InternValue{{.i = l.data.i * r.data.i}, InternValue::INT});
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        stack.push(InternValue{{.r = l.data.r * r.data.r}, InternValue::REAL});
      } else {
        has_error = true;
        error = "Type error in multiplication";
        l.dec();
        r.dec();
        goto exit;
      }
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::DIV: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (l.tag == InternValue::NIL || r.tag == InternValue::NIL) {
        stack.push(InternValue{{}, InternValue::NIL});
      } else if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        stack.push(InternValue{{.i = l.data.i / r.data.i}, InternValue::INT});
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        stack.push(InternValue{{.r = l.data.r / r.data.r}, InternValue::REAL});
      } else {
        has_error = true;
        error = "Type error in division";
        l.dec();
        r.dec();
        goto exit;
      }
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::MOD: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (l.tag == InternValue::NIL || r.tag == InternValue::NIL) {
        stack.push(InternValue{{}, InternValue::NIL});
      } else if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        stack.push(InternValue{{.i = l.data.i % r.data.i}, InternValue::INT});
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        stack.push(InternValue{{.r = std::fmod(l.data.r, r.data.r)},
                               InternValue::REAL});
      } else {
        has_error = true;
        error = "Type error in modulo";
        l.dec();
        r.dec();
        goto exit;
      }
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::SHL: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (!(l.tag == InternValue::INT && r.tag == InternValue::INT)) {
        has_error = true;
        error = "Type error in shift left";
        l.dec();
        r.dec();
        goto exit;
      }
      stack.push(InternValue{{.i = l.data.i << r.data.i}, InternValue::INT});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::SHR: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (!(l.tag == InternValue::INT && r.tag == InternValue::INT)) {
        has_error = true;
        error = "Type error in shift right";
        l.dec();
        r.dec();
        goto exit;
      }
      stack.push(InternValue{{.i = l.data.i >> r.data.i}, InternValue::INT});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::BIT_AND: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (!(l.tag == InternValue::INT && r.tag == InternValue::INT)) {
        has_error = true;
        error = "Type error in bitwise and";
        l.dec();
        r.dec();
        goto exit;
      }
      stack.push(InternValue{{.i = l.data.i & r.data.i}, InternValue::INT});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::BIT_OR: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (!(l.tag == InternValue::INT && r.tag == InternValue::INT)) {
        has_error = true;
        error = "Type error in bitwise or";
        l.dec();
        r.dec();
        goto exit;
      }
      stack.push(InternValue{{.i = l.data.i | r.data.i}, InternValue::INT});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::AND: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (!(l.tag == InternValue::BOOL && r.tag == InternValue::BOOL)) {
        has_error = true;
        error = "Type error in boolean and";
        l.dec();
        r.dec();
        goto exit;
      }
      stack.push(InternValue{{.b = l.data.b && r.data.b}, InternValue::BOOL});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::OR: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      if (!(l.tag == InternValue::BOOL && r.tag == InternValue::BOOL)) {
        has_error = true;
        error = "Type error in boolean or";
        l.dec();
        r.dec();
        goto exit;
      }
      stack.push(InternValue{{.b = l.data.b || r.data.b}, InternValue::BOOL});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::NEQ: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      stack.push(InternValue{{.b = l != r}, InternValue::BOOL});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::LT: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      bool res = false;
      if (l.tag == InternValue::NIL) {
        res = r.tag != InternValue::NIL;
      } else if (r.tag == InternValue::NIL) {
        res = false;
      } else if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        res = l.data.i < r.data.i;
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        res = l.data.r < r.data.r;
      } else if (l.tag == InternValue::DATE && r.tag == InternValue::DATE) {
        res = l.data.d < r.data.d;
      } else if (l.tag == InternValue::TIME && r.tag == InternValue::TIME) {
        res = l.data.t < r.data.t;
      } else {
        has_error = true;
        error = "Type error in less than";
        l.dec();
        r.dec();
        goto exit;
      }
      stack.push(InternValue{{.b = res}, InternValue::BOOL});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::LE: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      bool res = false;
      if (l.tag == InternValue::NIL) {
        res = true;
      } else if (r.tag == InternValue::NIL) {
        res = false;
      } else if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        res = l.data.i <= r.data.i;
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        res = l.data.r <= r.data.r;
      } else if (l.tag == InternValue::DATE && r.tag == InternValue::DATE) {
        res = l.data.d <= r.data.d;
      } else if (l.tag == InternValue::TIME && r.tag == InternValue::TIME) {
        res = l.data.t <= r.data.t;
      } else {
        has_error = true;
        error = "Type error in less than or equal";
        l.dec();
        r.dec();
        goto exit;
      }
      stack.push(InternValue{{.b = res}, InternValue::BOOL});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::GT: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      bool res = false;
      if (l.tag == InternValue::NIL) {
        res = r.tag != InternValue::NIL;
      } else if (r.tag == InternValue::NIL) {
        res = true;
      } else if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        res = l.data.i > r.data.i;
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        res = l.data.r > r.data.r;
      } else if (l.tag == InternValue::DATE && r.tag == InternValue::DATE) {
        res = l.data.d > r.data.d;
      } else if (l.tag == InternValue::TIME && r.tag == InternValue::TIME) {
        res = l.data.t > r.data.t;
      } else {
        has_error = true;
        error = "Type error in greater than";
        l.dec();
        r.dec();
        goto exit;
      }
      stack.push(InternValue{{.b = res}, InternValue::BOOL});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::GE: {
      auto r = stack.top();
      stack.pop();
      auto l = stack.top();
      stack.pop();
      bool res = false;
      if (l.tag == InternValue::NIL) {
        res = r.tag == InternValue::NIL;
      } else if (r.tag == InternValue::NIL) {
        res = true;
      } else if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        res = l.data.i >= r.data.i;
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        res = l.data.r >= r.data.r;
      } else if (l.tag == InternValue::DATE && r.tag == InternValue::DATE) {
        res = l.data.d >= r.data.d;
      } else if (l.tag == InternValue::TIME && r.tag == InternValue::TIME) {
        res = l.data.t >= r.data.t;
      } else {
        has_error = true;
        error = "Type error in greater than or equal";
        l.dec();
        r.dec();
        goto exit;
      }
      stack.push(InternValue{{.b = res}, InternValue::BOOL});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::LOAD_AT: {
      std::uint32_t id = instr.arg.u;
      std::uint16_t depth = instr.ext;
      InternValue v;
      if (depth == 1) {
        v = vals[id];
      } else {
        v = histories[id].get(depth - 2);
      }
      v.inc();
      stack.push(v);
      ip++;
    } break;
    case Op::DISPATCH: {
      dispatches.push_back(instr.arg.u);
      ip++;
    } break;
    case Op::CTOR: {
      std::uint16_t argc = instr.ext;
      std::uint32_t event_tag = instr.arg.u;
      std::vector<InternValue> args(argc);
      for (int i = argc - 1; i >= 0; --i) {
        args[i] = stack.top();
        stack.pop();
      }
      stack.push(InternValue::of_event(event_tag, args));
      ip++;
    } break;
    case Op::CALL: {
      std::uint16_t argc = instr.ext;
      std::vector<InternValue> args(argc);
      for (int i = argc - 1; i >= 0; --i) {
        args[i] = stack.top();
        stack.pop();
      }
      if (exts[instr.arg.u]) {
        auto res = exts[instr.arg.u](argc, args.data());
        stack.push(res);
      } else {
        stack.push({{}, InternValue::NIL});
      }
      for (auto &arg : args) {
        arg.dec();
      }
      ip++;
    } break;
    }
  }
exit:
}

void Interpreter::dispatch() {
  for (auto &sig : dispatches) {
    if (vals[sig].tag != InternValue::EVENT) {
      continue;
    }

    auto &e = *vals[sig].data.e;
    if (e.tag < listeners.size() && listeners[e.tag]) {
      vals[sig].inc();
      listeners[e.tag](e.argc, e.args);
      vals[sig].dec();
    }
  }
  dispatches.clear();
}

void Interpreter::commit() {
  for (std::uint32_t i = 0; i < vals.size(); ++i) {
    if (dirty[i]) {
      histories[i].push(vals[i]);
      vals[i] = current[i];
    } else {
      vals[i].inc();
      histories[i].push(vals[i]);
    }
  }
  std::fill(current.begin(), current.end(), InternValue{{}, InternValue::NIL});
}

void Interpreter::cleanup() {
  ip = 0;
  while (!stack.empty()) {
    stack.top().dec();
    stack.pop();
  }
  for (auto &loc : locals) {
    loc.dec();
  }
  locals.clear();
  std::fill(dirty.begin(), dirty.end(), false);
}

Interpreter::~Interpreter() {
  cleanup();

  for (auto &v : vals) {
    v.dec();
  }

  for (auto &v : consts) {
    v.dec();
  }
}

std::vector<std::uint16_t> depths_of(const Program &prog) {
  std::vector<std::uint16_t> depths(prog.var_count, 0);

  for (const auto &instr : prog.instrs) {
    if (instr.kind == Op::LOAD_AT) {
      depths[instr.arg.u] = std::max(depths[instr.arg.u], instr.ext);
    }
  }

  return depths;
}

// TODO: Proper implementation
void IPQueue::push(Rank r, std::size_t ip) {
  for (auto p : ptrs) {
    if (p.second == ip)
      return;
  }

  min = std::min(r, min);
  ptrs.push_back({r, ip});
}

std::pair<IPQueue::Rank, std::size_t> IPQueue::front() {
  for (auto &p : ptrs) {
    if (p.first == min)
      return p;
  }
  assert(false && "Empty queue");
}

bool IPQueue::empty() { return ptrs.empty(); }

void IPQueue::pop() {
  auto it = std::find_if(ptrs.begin(), ptrs.end(),
                         [this](auto &&p) { return p.first == min; });

  if (it == ptrs.end())
    assert(false && "Empty queue");

  ptrs.erase(it);

  min = static_cast<Rank>(-1);
  for (auto p : ptrs) {
    min = std::min(min, p.first);
  }
}

void InternValue::inc() {
  switch (tag) {
  case STRING:
    data.s->rc++;
    break;
  case EVENT:
    data.e->rc++;
    break;
  case NIL:
  case INT:
  case REAL:
  case BOOL:
  case DATE:
  case TIME:
    break;
  }
}

void InternValue::dec() {
  switch (tag) {
  case STRING:
    data.s->rc--;
    if (data.s->rc == 0) {
      destroy();
    }
    break;
  case EVENT:
    data.e->rc--;
    if (data.e->rc == 0) {
      destroy();
    }
    break;
  case NIL:
  case INT:
  case REAL:
  case BOOL:
  case DATE:
  case TIME:
    break;
  }
}

void InternValue::destroy() {
  switch (tag) {
  case STRING:
    data.s->~InternString();
    std::free(data.s);
    break;
  case EVENT:
    for (std::uint16_t i = 0; i < data.e->argc; ++i) {
      data.e->args[i].dec();
    }
    data.e->~InternEvent();
    std::free(data.e);
    break;
  case NIL:
  case INT:
  case REAL:
  case BOOL:
  case DATE:
  case TIME:
    break;
  }
}

InternValue InternValue::of_string(std::string_view s) {
  void *mem = std::malloc(offsetof(InternString, data) + s.size() + 1);
  auto str = new (mem) InternString{1, s.size()};
  std::memcpy(str->data, s.data(), s.size());
  str->data[s.size()] = '\0';
  return {{.s = str}, STRING};
}

InternValue InternValue::of_event(std::uint32_t event_tag,
                                  std::span<const InternValue> args) {
  void *mem = std::malloc(offsetof(InternEvent, args) +
                          sizeof(InternValue) * args.size());
  auto ev = new (mem)
      InternEvent{1, event_tag, static_cast<std::uint16_t>(args.size())};
  for (std::size_t i = 0; i < args.size(); ++i) {
    ev->args[i] = args[i];
  }
  return {{.e = ev}, EVENT};
}

const std::string_view InternValue::as_string() const {
  assert(tag == STRING);
  std::string_view s{data.s->data, data.s->len};
  return s;
}

bool InternValue::operator==(const InternValue &other) const {
  if (tag != other.tag)
    return false;
  switch (tag) {
  case NIL:
    return true;
  case INT:
    return data.i == other.data.i;
  case REAL:
    return data.r == other.data.r;
  case BOOL:
    return data.b == other.data.b;
  case DATE:
    return data.d == other.data.d;
  case TIME:
    return data.t == other.data.t;
  case STRING:
    return as_string() == other.as_string();
  case EVENT: {
    if (data.e->tag != other.data.e->tag || data.e->argc != other.data.e->argc)
      return false;
    for (std::uint16_t i = 0; i < data.e->argc; ++i) {
      if (data.e->args[i] != other.data.e->args[i])
        return false;
    }
    return true;
  } break;
  }
}

void RingBuffer::init(std::size_t cap) {
  capacity = cap;
  buffer.resize(capacity, InternValue{{}, InternValue::NIL});
  head = 0;
}

void RingBuffer::push(InternValue val) {
  if (capacity == 0) {
    val.dec();
    return;
  }
  head = (head + capacity - 1) % capacity;
  buffer[head].dec();
  buffer[head] = val;
}

InternValue RingBuffer::get(std::size_t depth) const {
  if (capacity == 0)
    return InternValue{{}, InternValue::NIL};
  std::size_t idx = (head + depth) % capacity;
  return buffer[idx];
}

RingBuffer::~RingBuffer() {
  for (auto &v : buffer) {
    v.dec();
  }
}

}; // namespace mold::internal
