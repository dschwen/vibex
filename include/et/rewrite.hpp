#pragma once
#include <vector>
#include <functional>
#include <utility>

#include "et/runtime_ast.hpp"
#include "et/normalize.hpp"
#include "et/match.hpp"

namespace et {

struct Rule {
  pat::Pattern lhs;
  pat::Pattern rhs;
  std::function<bool(const RGraph&, const Bindings&)> guard; // optional
  const char* name = "";
  int priority = 0;
};

// Clone subtree from src graph into dst graph (memoized to preserve sharing)
inline int clone_subtree(const RGraph& src, RGraph& dst, int id, std::unordered_map<int,int>& memo) {
  auto it = memo.find(id);
  if (it != memo.end()) return it->second;
  const RNode& n = src.nodes[id];
  RNode nn; nn.kind = n.kind; nn.cval = n.cval; nn.var_index = n.var_index;
  nn.ch.reserve(n.ch.size());
  for (int cid : n.ch) nn.ch.push_back(clone_subtree(src, dst, cid, memo));
  int nid = dst.add(std::move(nn));
  memo.emplace(id, nid);
  return nid;
}

// Instantiate RHS pattern into a new subtree in dst graph according to bindings over src
inline int instantiate_rhs(const pat::Pattern& p, const RGraph& src, const Bindings& b, RGraph& dst,
                           std::unordered_map<int,int>& memo_clone) {
  using Kind = pat::Pattern::Kind;
  if (p.kind == Kind::Placeholder) {
    auto it = b.find(p.placeholder_id);
    if (it == b.end()) return -1; // invalid
    return clone_subtree(src, dst, it->second, memo_clone);
  }
  // Concrete node
  RNode n; n.kind = p.node_kind;
  if (n.kind == NodeKind::Const) { n.cval = p.cval; }
  if (n.kind == NodeKind::Var)   { n.var_index = p.var_index; }
  n.ch.reserve(p.ch.size());
  for (const auto& c : p.ch) n.ch.push_back(instantiate_rhs(c, src, b, dst, memo_clone));
  return dst.add(std::move(n));
}

// Rewrite a single node (postorder) into dst graph; returns dst node id
inline int rewrite_node(const RGraph& src, int id, const std::vector<Rule>& rules,
                        RGraph& dst) {
  const RNode& n = src.nodes[id];

  // First, rewrite children to dst ids
  std::vector<int> ch; ch.reserve(n.ch.size());
  for (int cid : n.ch) ch.push_back(rewrite_node(src, cid, rules, dst));

  // Try rules at this node (on the original shape, but we could also match against normalized children)
  for (const auto& r : rules) {
    Bindings bind;
    if (match_node(src, id, r.lhs, bind)) {
      if (!r.guard || r.guard(src, bind)) {
        std::unordered_map<int,int> memo_clone;
        int rid = instantiate_rhs(r.rhs, src, bind, dst, memo_clone);
        return rid;
      }
    }
  }

  // No rule matched: rebuild node with rewritten children
  RNode nn; nn.kind = n.kind; nn.cval = n.cval; nn.var_index = n.var_index; nn.ch = std::move(ch);
  return dst.add(std::move(nn));
}

inline RGraph apply_rules_once(const RGraph& g, const std::vector<Rule>& rules) {
  // Sort rules by priority desc, stable
  std::vector<const Rule*> order; order.reserve(rules.size());
  for (auto& r : rules) order.push_back(&r);
  std::stable_sort(order.begin(), order.end(), [](const Rule* a, const Rule* b){ return a->priority > b->priority; });
  // Build a vector in sorted order
  std::vector<Rule> sorted; sorted.reserve(rules.size());
  for (auto* pr : order) sorted.push_back(*pr);

  RGraph dst;
  dst.root = rewrite_node(g, g.root, sorted, dst);
  return dst;
}

inline RGraph rewrite_fixed_point(const RGraph& g0, const std::vector<Rule>& rules, int max_passes = 5) {
  RGraph prev = g0;
  for (int i = 0; i < max_passes; ++i) {
    RGraph cur = apply_rules_once(prev, rules);
    // Normalize to canonical form between passes
    cur = normalize(cur);
    if (cur.nodes.size() == prev.nodes.size() && r_equal(cur, cur.root, prev.root)) return cur;
    prev = std::move(cur);
  }
  return prev;
}

// High-level: ET -> runtime -> normalize -> rewrite* -> normalize -> ET
template <class Expr>
inline RGraph rewrite_expr(const Expr& e, const std::vector<Rule>& rules) {
  RGraph g = compile_to_runtime(e);
  g = normalize(g);
  g = rewrite_fixed_point(g, rules, 6);
  return normalize(g);
}

} // namespace et
