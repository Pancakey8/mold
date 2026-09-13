#include "interpreter.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <print>
#include <variant>

std::vector<std::uint16_t> depths_of(const Program &prog);

void Interpreter::init() {
  vals.resize(prog.glob_size);
  dirty.resize(prog.glob_size);
  consts.reserve(prog.consts.size());
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
  // TODO: History
}

void Interpreter::set_input(std::string_view name, InternValue value) {
  if (auto it = prog.inputs.find(name); it != prog.inputs.end()) {
    if (vals[it->second] != value) {
      dirty[it->second] = true;
    }
    vals[it->second].dec();
    vals[it->second] = value;
  } else {
    // TODO: Fail this?
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
      } else if (l.tag == InternValue::INT) {
        stack.push(InternValue{{.i = l.data.i + r.data.i}, InternValue::INT});
      } else if (l.tag == InternValue::REAL) {
        stack.push(InternValue{{.r = l.data.r + r.data.r}, InternValue::REAL});
      } else if (l.tag == InternValue::STRING) {
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
      if (vals[instr.arg.u] != v)
        dirty[instr.arg.u] = true;
      vals[instr.arg.u].dec();
      vals[instr.arg.u] = v;
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
    case Op::DISPATCH:
    case Op::CTOR:
    case Op::CALL:
    case Op::LOAD_AT:
    case Op::MOD:
    case Op::DIV:
    case Op::MULT:
    case Op::SUB:
    case Op::SHR:
    case Op::SHL:
    case Op::GE:
    case Op::LE:
    case Op::GT:
    case Op::LT:
    case Op::NEQ:
    case Op::BIT_OR:
    case Op::BIT_AND:
    case Op::OR:
    case Op::AND:
      break;
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
  std::vector<std::uint16_t> depths(prog.glob_size, 0);

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
  }
}
