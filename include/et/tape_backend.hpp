#pragma once
#include <vector>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <type_traits>

namespace et {

struct Tape {
  enum Kind : uint8_t { KVar, KConst, KAdd, KSub, KMul, KDiv, KPow, KNeg, KSin, KExp, KLog, KSqrt, KTanh, KCos,
#ifdef ET_ENABLE_CONTROL_FLOW
                        KLt, KLe, KGt, KGe, KEq, KNe, KNot, KIf, KSelect,
#endif
  };

  struct Node {
    Kind kind;
    int  a = -1;
    int  b = -1;
    double c = 0;
    std::size_t var_index = ~std::size_t(0);
  };

  std::vector<Node> nodes;
  int output_id = -1;

  double forward(const std::vector<double>& inputs) const {
    std::vector<double> val(nodes.size());
    for (int i = 0; i < (int)nodes.size(); ++i) {
      const auto& n = nodes[i];
      switch (n.kind) {
        case KVar:  val[i] = inputs[n.var_index]; break;
        case KConst:val[i] = n.c; break;
        case KAdd:  val[i] = val[n.a] + val[n.b]; break;
        case KSub:  val[i] = val[n.a] - val[n.b]; break;
        case KMul:  val[i] = val[n.a] * val[n.b]; break;
        case KDiv:  val[i] = val[n.a] / val[n.b]; break;
        case KPow:  val[i] = std::pow(val[n.a], val[n.b]); break;
        case KNeg:  val[i] = -val[n.a]; break;
        case KSin:  val[i] = std::sin(val[n.a]); break;
        case KExp:  val[i] = std::exp(val[n.a]); break;
        case KLog:  val[i] = std::log(val[n.a]); break;
        case KSqrt: val[i] = std::sqrt(val[n.a]); break;
        case KTanh: val[i] = std::tanh(val[n.a]); break;
        case KCos:  val[i] = std::cos(val[n.a]); break;
#ifdef ET_ENABLE_CONTROL_FLOW
        case KLt:     val[i] = val[n.a] <  val[n.b] ? 1.0 : 0.0; break;
        case KLe:     val[i] = val[n.a] <= val[n.b] ? 1.0 : 0.0; break;
        case KGt:     val[i] = val[n.a] >  val[n.b] ? 1.0 : 0.0; break;
        case KGe:     val[i] = val[n.a] >= val[n.b] ? 1.0 : 0.0; break;
        case KEq:     val[i] = val[n.a] == val[n.b] ? 1.0 : 0.0; break;
        case KNe:     val[i] = val[n.a] != val[n.b] ? 1.0 : 0.0; break;
        case KNot:    val[i] = (val[n.a] == 0.0) ? 1.0 : 0.0; break;
        case KIf:     val[i] = (val[n.a] != 0.0) ? val[n.b] : val[n.c]; break;
        case KSelect: val[i] = (val[n.a] != 0.0) ? val[n.b] : val[n.c]; break;
#endif
      }
    }
    return val[output_id];
  }

  std::vector<double> backward(const std::vector<double>& inputs) const {
    const int N = (int)nodes.size();
    std::vector<double> val(N), bar(N, 0.0);
    for (int i = 0; i < N; ++i) {
      const auto& n = nodes[i];
      switch (n.kind) {
        case KVar:  val[i] = inputs[n.var_index]; break;
        case KConst:val[i] = n.c; break;
        case KAdd:  val[i] = val[n.a] + val[n.b]; break;
        case KSub:  val[i] = val[n.a] - val[n.b]; break;
        case KMul:  val[i] = val[n.a] * val[n.b]; break;
        case KDiv:  val[i] = val[n.a] / val[n.b]; break;
        case KPow:  val[i] = std::pow(val[n.a], val[n.b]); break;
        case KNeg:  val[i] = -val[n.a]; break;
        case KSin:  val[i] = std::sin(val[n.a]); break;
        case KExp:  val[i] = std::exp(val[n.a]); break;
        case KLog:  val[i] = std::log(val[n.a]); break;
        case KSqrt: val[i] = std::sqrt(val[n.a]); break;
        case KTanh: val[i] = std::tanh(val[n.a]); break;
        case KCos:  val[i] = std::cos(val[n.a]); break;
#ifdef ET_ENABLE_CONTROL_FLOW
        case KLt:     val[i] = val[n.a] <  val[n.b] ? 1.0 : 0.0; break;
        case KLe:     val[i] = val[n.a] <= val[n.b] ? 1.0 : 0.0; break;
        case KGt:     val[i] = val[n.a] >  val[n.b] ? 1.0 : 0.0; break;
        case KGe:     val[i] = val[n.a] >= val[n.b] ? 1.0 : 0.0; break;
        case KEq:     val[i] = val[n.a] == val[n.b] ? 1.0 : 0.0; break;
        case KNe:     val[i] = val[n.a] != val[n.b] ? 1.0 : 0.0; break;
        case KNot:    val[i] = (val[n.a] == 0.0) ? 1.0 : 0.0; break;
        case KIf:     val[i] = (val[n.a] != 0.0) ? val[n.b] : val[n.c]; break;
        case KSelect: val[i] = (val[n.a] != 0.0) ? val[n.b] : val[n.c]; break;
#endif
      }
    }
    bar[output_id] = 1.0;
    for (int i = N - 1; i >= 0; --i) {
      const auto& n = nodes[i];
      switch (n.kind) {
        case KVar:   break;
        case KConst: break;
        case KAdd:
          bar[n.a] += bar[i];
          bar[n.b] += bar[i];
          break;
        case KSub:
          bar[n.a] += bar[i];
          bar[n.b] -= bar[i];
          break;
        case KMul:
          bar[n.a] += bar[i] * val[n.b];
          bar[n.b] += bar[i] * val[n.a];
          break;
        case KDiv:
          bar[n.a] += bar[i] / val[n.b];
          bar[n.b] -= bar[i] * val[n.a] / (val[n.b] * val[n.b]);
          break;
        case KPow: {
          double f = std::pow(val[n.a], val[n.b]);
          bar[n.a] += bar[i] * f * (val[n.b] / val[n.a]);
          bar[n.b] += bar[i] * f * std::log(val[n.a]);
          break; }
        case KNeg:
          bar[n.a] -= bar[i];
          break;
        case KSin:
          bar[n.a] += bar[i] * std::cos(val[n.a]);
          break;
        case KExp:
          bar[n.a] += bar[i] * std::exp(val[n.a]);
          break;
        case KLog:
          bar[n.a] += bar[i] / val[n.a];
          break;
        case KSqrt:
          bar[n.a] += bar[i] * (0.5 / std::sqrt(val[n.a]));
          break;
        case KTanh: {
          double t = std::tanh(val[n.a]);
          bar[n.a] += bar[i] * (1.0 - t * t);
          break; }
        case KCos:
          bar[n.a] -= bar[i] * std::sin(val[n.a]);
          break;
#ifdef ET_ENABLE_CONTROL_FLOW
        case KLt: case KLe: case KGt: case KGe: case KEq: case KNe: case KNot:
          // No gradient into predicate inputs
          break;
        case KIf: {
          bool cond = (val[n.a] != 0.0);
          if (cond) bar[n.b] += bar[i]; else bar[n.c] += bar[i];
          break;
        }
        case KSelect: {
          bool cond = (val[n.a] != 0.0);
          if (cond) bar[n.b] += bar[i]; else bar[n.c] += bar[i];
          break;
        }
#endif
      }
    }
    std::size_t arity = 0;
    for (auto& n : nodes) if (n.kind == KVar) arity = std::max(arity, n.var_index+1);
    std::vector<double> grad(arity, 0.0);
    for (int i = 0; i < N; ++i)
      if (nodes[i].kind == KVar) grad[nodes[i].var_index] += bar[i];
    return grad;
  }

};

struct TapeBackend {
  using result_type = int;
  Tape tape;

  explicit TapeBackend(std::size_t /*arity*/) { tape.nodes.reserve(64); }

  template <class T>
  result_type emitVar(std::size_t idx) {
    Tape::Node n; n.kind = Tape::KVar; n.var_index = idx;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  template <class T>
  result_type emitConst(Const<T> c) {
    Tape::Node n; n.kind = Tape::KConst; n.c = static_cast<double>(c.value);
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  template <class Op>
  result_type emitApply(Op, int a) {
    Tape::Node n;
    if constexpr (std::is_same<Op, NegOp>::value) n.kind = Tape::KNeg;
    else if constexpr (std::is_same<Op, SinOp>::value) n.kind = Tape::KSin;
    else if constexpr (std::is_same<Op, ExpOp>::value) n.kind = Tape::KExp;
    else if constexpr (std::is_same<Op, LogOp>::value) n.kind = Tape::KLog;
    else if constexpr (std::is_same<Op, SqrtOp>::value) n.kind = Tape::KSqrt;
    else if constexpr (std::is_same<Op, TanhOp>::value) n.kind = Tape::KTanh;
    else if constexpr (std::is_same<Op, CosOp>::value) n.kind = Tape::KCos;
    #ifdef ET_ENABLE_CONTROL_FLOW
    else if constexpr (std::is_same<Op, NotOp>::value) n.kind = Tape::KNot;
    #endif
    else static_assert(!std::is_same<Op,Op>::value, "Unary op not mapped to Tape");
    n.a = a;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

  template <class Op>
  result_type emitApply(Op, int a, int b) {
    Tape::Node n;
    if constexpr      (std::is_same<Op, AddOp>::value) n.kind = Tape::KAdd;
    else if constexpr (std::is_same<Op, SubOp>::value) n.kind = Tape::KSub;
    else if constexpr (std::is_same<Op, MulOp>::value) n.kind = Tape::KMul;
    else if constexpr (std::is_same<Op, DivOp>::value) n.kind = Tape::KDiv;
    else if constexpr (std::is_same<Op, PowOp>::value) n.kind = Tape::KPow;
    #ifdef ET_ENABLE_CONTROL_FLOW
    else if constexpr (std::is_same<Op, LtOp>::value) n.kind = Tape::KLt;
    else if constexpr (std::is_same<Op, LeOp>::value) n.kind = Tape::KLe;
    else if constexpr (std::is_same<Op, GtOp>::value) n.kind = Tape::KGt;
    else if constexpr (std::is_same<Op, GeOp>::value) n.kind = Tape::KGe;
    else if constexpr (std::is_same<Op, EqOp>::value) n.kind = Tape::KEq;
    else if constexpr (std::is_same<Op, NeOp>::value) n.kind = Tape::KNe;
    #endif
    else static_assert(!std::is_same<Op,Op>::value, "Binary op not mapped to Tape");
    n.a = a; n.b = b;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }

#ifdef ET_ENABLE_CONTROL_FLOW
  template <class Op>
  result_type emitApply(Op, int a, int b, int c) {
    Tape::Node n;
    if constexpr (std::is_same<Op, IfOp>::value) n.kind = Tape::KIf;
    else if constexpr (std::is_same<Op, SelectOp>::value) n.kind = Tape::KSelect;
    else static_assert(!std::is_same<Op,Op>::value, "Ternary op not mapped to Tape");
    n.a = a; n.b = b; n.c = c;
    tape.nodes.push_back(n);
    return (int)tape.nodes.size() - 1;
  }
#endif
};

} // namespace et
