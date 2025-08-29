#pragma once
#include "et/ast.hpp"
#include "et/expr.hpp"
#include "et/tape_backend.hpp"

namespace et {

// Alias: keep the same implementation as before
inline int compile_runtime(const Expr& e, TapeBackend& b) {
  std::function<int(const std::shared_ptr<Node>&)> rec;
  rec = [&](const std::shared_ptr<Node>& p) -> int {
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
    if (auto q = std::dynamic_pointer_cast<LtNode>(p))    return b.emitApply(LtOp{},  rec(q->a), rec(q->b));
    if (auto q = std::dynamic_pointer_cast<LeNode>(p))    return b.emitApply(LeOp{},  rec(q->a), rec(q->b));
    if (auto q = std::dynamic_pointer_cast<GtNode>(p))    return b.emitApply(GtOp{},  rec(q->a), rec(q->b));
    if (auto q = std::dynamic_pointer_cast<GeNode>(p))    return b.emitApply(GeOp{},  rec(q->a), rec(q->b));
    if (auto q = std::dynamic_pointer_cast<EqNode>(p))    return b.emitApply(EqOp{},  rec(q->a), rec(q->b));
    if (auto q = std::dynamic_pointer_cast<NeNode>(p))    return b.emitApply(NeOp{},  rec(q->a), rec(q->b));
    if (auto q = std::dynamic_pointer_cast<NotNode>(p))   return b.emitApply(NotOp{}, rec(q->a));
    if (auto q = std::dynamic_pointer_cast<IfNode>(p))    return b.emitApply(IfOp{}, rec(q->c), rec(q->t), rec(q->e));
    if (auto q = std::dynamic_pointer_cast<SelectNode>(p))return b.emitApply(SelectOp{}, rec(q->m), rec(q->t), rec(q->e));
    if (auto q = std::dynamic_pointer_cast<IterNode>(p)) return b.emitIter();
    if (auto q = std::dynamic_pointer_cast<StateReadNode>(p)) return b.emitStateRead(q->index);
    if (auto q = std::dynamic_pointer_cast<LoopForNode>(p)) {
      std::vector<int> chids; chids.reserve(q->ch.size());
      for (auto& c : q->ch) chids.push_back(rec(c));
      return b.emitLoopFor(q->K, chids);
    }
    if (auto q = std::dynamic_pointer_cast<LoopOutNode>(p)) {
      int loop_id = rec(q->loop);
      return b.emitLoopOut(q->J, loop_id);
    }
    return b.emitConst(0.0);
  };
  return rec(e.n);
}

} // namespace et

