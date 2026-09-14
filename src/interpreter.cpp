#include "interpreter.hpp"
#include "utils.hpp"
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
  consts.reserve(prog.consts.size());
  exts.resize(prog.ext_count);
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
      vals[i].inc();
      histories[i].push(vals[i]);
    }
  }
}

void Interpreter::store(std::uint32_t id, InternValue value) {
  if (vals[id] != value) {
    dirty[id] = true;
  }
  vals[id].dec();
  vals[id] = value;

  value.inc();
  histories[id].push(value);
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
      stack.push(InternValue{{.b = dirty[instr.arg.u]}, InternValue::BOOL});
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
      } else {
        assert(false && "Unreachable?");
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
      auto v = vals[instr.arg.u];
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
      } else {
        assert(false && "Unreachable?");
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
        assert(false && "Unreachable?");
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
        assert(false && "Unreachable?");
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
        assert(false && "Unreachable?");
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
      assert(l.tag == InternValue::INT && r.tag == InternValue::INT);
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
      assert(l.tag == InternValue::INT && r.tag == InternValue::INT);
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
      assert(l.tag == InternValue::INT && r.tag == InternValue::INT);
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
      assert(l.tag == InternValue::INT && r.tag == InternValue::INT);
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
      assert(l.tag == InternValue::BOOL && r.tag == InternValue::BOOL);
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
      assert(l.tag == InternValue::BOOL && r.tag == InternValue::BOOL);
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
      if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        res = l.data.i < r.data.i;
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        res = l.data.r < r.data.r;
      } else {
        assert(false && "Unreachable?");
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
      if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        res = l.data.i <= r.data.i;
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        res = l.data.r <= r.data.r;
      } else {
        assert(false && "Unreachable?");
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
      if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        res = l.data.i > r.data.i;
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        res = l.data.r > r.data.r;
      } else {
        assert(false && "Unreachable?");
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
      if (l.tag == InternValue::INT && r.tag == InternValue::INT) {
        res = l.data.i >= r.data.i;
      } else if (l.tag == InternValue::REAL && r.tag == InternValue::REAL) {
        res = l.data.r >= r.data.r;
      } else {
        assert(false && "Unreachable?");
      }
      stack.push(InternValue{{.b = res}, InternValue::BOOL});
      l.dec();
      r.dec();
      ip++;
    } break;
    case Op::LOAD_AT: {
      std::uint32_t id = instr.arg.u;
      std::uint16_t depth = instr.ext;
      auto v = histories[id].get(depth);
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
        assert(false && "TODO: Error handling");
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
  capacity = cap + 1;
  buffer.resize(capacity, InternValue{{}, InternValue::NIL});
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
