#pragma once
#include "et/ast.hpp"
// Include legacy expr tags to satisfy TapeBackend templated overloads
#include "et/expr.hpp"
#include "et/tape_backend.hpp"

namespace et {

inline int compile_runtime(const Expr& e, TapeBackend& b) {
  // Recursive lambda over shared_ptr<Node>
  std::function<int(const std::shared_ptr<Node>&)> rec;
  rec = [&](const std::shared_ptr<Node>& p) -> int {
    // Order checks from most common
    if (auto q = std::dynamic_pointer_cast<ConstNode>(p)) return b.emitConst(q->value);
    if (auto q = std::dynamic_pointer_cast<VarNode>(p))   return b.emitVar(q->index);
    if (auto q = std::dynamic_pointer_cast<NegNode>(p))   return b.emitNeg(rec(q->a));
    if (auto q = std::dynamic_pointer_cast<SinNode>(p))   return b.emitSin(rec(q->a));
    if (auto q = std::dynamic_pointer_cast<CosNode>(p))   return b.emitCos(rec(q->a));
    if (auto q = std::dynamic_pointer_cast<ExpNode>(p))   return b.emitExp(rec(q->a));
    if (auto q = std::dynamic_pointer_cast<LogNode>(p))   return b.emitLog(rec(q->a));
    if (auto q = std::dynamic_pointer_cast<SqrtNode>(p))  return b.emitSqrt(rec(q->a));
    if (auto q = std::dynamic_pointer_cast<TanhNode>(p))  return b.emitTanh(rec(q->a));
    if (auto q = std::dynamic_pointer_cast<AddNode>(p))   return b.emitAdd(rec(q->a), rec(q->b));
    if (auto q = std::dynamic_pointer_cast<SubNode>(p))   return b.emitSub(rec(q->a), rec(q->b));
    if (auto q = std::dynamic_pointer_cast<MulNode>(p))   return b.emitMul(rec(q->a), rec(q->b));
    if (auto q = std::dynamic_pointer_cast<DivNode>(p))   return b.emitDiv(rec(q->a), rec(q->b));
    if (auto q = std::dynamic_pointer_cast<PowNode>(p))   return b.emitPow(rec(q->a), rec(q->b));
    // Fallback: constant zero
    return b.emitConst(0.0);
  };
  return rec(e.n);
}

} // namespace et
