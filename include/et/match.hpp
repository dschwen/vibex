#pragma once
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "et/runtime_ast.hpp"
#include "et/pattern.hpp"

namespace et {

using Bindings = std::unordered_map<int,int>; // pid -> single node id
using MultiBindings = std::unordered_map<int, std::vector<int>>; // pid -> list of node ids (for spreads)

inline bool is_ac(NodeKind k) { return k == NodeKind::Add || k == NodeKind::Mul; }

// Forward decl
bool match_node(const RGraph& g, int id, const pat::Pattern& p, Bindings& b, MultiBindings& mb);

// AC multiset matching with backtracking
inline bool match_ac(const RGraph& g, const RNode& n, const pat::Pattern& p, Bindings& b, MultiBindings& mb) {
  if (n.kind != p.node_kind) return false;
  // Spreads allowed: at most one spread captures the remainder. Without spread, require exact cover.
  std::size_t spreads = 0; std::size_t spread_idx = ~std::size_t(0);
  for (std::size_t i=0;i<p.ch.size();++i) if (p.ch[i].kind==pat::Pattern::Kind::Placeholder && p.ch[i].is_spread) { spreads++; spread_idx = i; }
  if (spreads > 1) return false;
  if (spreads == 0 && p.ch.size() != n.ch.size()) return false;
  if (spreads == 1 && p.ch.size()-1 > n.ch.size()) return false;

  // Order pattern children by decreasing specificity, excluding spread
  std::vector<std::size_t> pidx; pidx.reserve(p.ch.size());
  for (std::size_t i = 0; i < pidx.size(); ++i) pidx[i] = i;
  pidx.clear();
  for (std::size_t i=0;i<p.ch.size();++i) if (!(p.ch[i].kind==pat::Pattern::Kind::Placeholder && p.ch[i].is_spread)) pidx.push_back(i);
  std::sort(pidx.begin(), pidx.end(), [&](std::size_t a, std::size_t c){
    return pat::specificity(p.ch[a]) > pat::specificity(p.ch[c]);
  });

  // Remaining candidate children of n (ids)
  std::vector<int> remaining = n.ch;

  std::function<bool(std::size_t)> dfs = [&](std::size_t i) -> bool {
    if (i == pidx.size()) return true;
    const pat::Pattern& pc = p.ch[pidx[i]];
    for (std::size_t r = 0; r < remaining.size(); ++r) {
      int cand = remaining[r];
      // Snapshot bindings for backtracking
      auto b_snapshot = b; auto mb_snapshot = mb;
      if (match_node(g, cand, pc, b, mb)) {
        // consume cand
        int last = remaining.back(); remaining[r] = last; remaining.pop_back();
        if (dfs(i+1)) return true;
        // backtrack
        remaining.push_back(last); // restore size; position will be re-evaluated safely as we return
      }
      b = std::move(b_snapshot); mb = std::move(mb_snapshot);
    }
    return false;
  };

  bool ok = dfs(0);
  if (!ok) return false;
  // Assign spread binding to remainder if present
  if (spreads == 1) {
    const auto& sp = p.ch[spread_idx];
    auto it = mb.find(sp.placeholder_id);
    if (it == mb.end()) {
      mb.emplace(sp.placeholder_id, remaining);
    } else {
      // Must be identical by structure and size
      const auto& prev = it->second;
      if (prev.size() != remaining.size()) return false;
      for (std::size_t i=0;i<prev.size();++i) if (!r_equal(g, prev[i], remaining[i])) return false;
    }
  } else if (!remaining.empty()) {
    return false;
  }
  return true;
}

inline bool match_node(const RGraph& g, int id, const pat::Pattern& p, Bindings& b, MultiBindings& mb) {
  if (p.kind == pat::Pattern::Kind::Placeholder) {
    if (p.is_spread) {
      // Spread outside AC context unsupported: treat as single binding
      auto itv = mb.find(p.placeholder_id);
      if (itv == mb.end()) { mb.emplace(p.placeholder_id, std::vector<int>{id}); return true; }
      const auto& vec = itv->second;
      return vec.size()==1 && r_equal(g, vec[0], id);
    } else {
      auto it = b.find(p.placeholder_id);
      if (it == b.end()) { b.emplace(p.placeholder_id, id); return true; }
      return r_equal(g, it->second, id);
    }
  }

  const RNode& n = g.nodes[id];
  if (n.kind != p.node_kind) return false;

  if (is_ac(n.kind)) {
    return match_ac(g, n, p, b, mb);
  }

  // Non-AC: arity must match
  if (n.ch.size() != p.ch.size()) return false;
  for (std::size_t i = 0; i < n.ch.size(); ++i) {
    if (!match_node(g, n.ch[i], p.ch[i], b, mb)) return false;
  }
  return true;
}

inline bool match(const RGraph& g, const pat::Pattern& p, Bindings& b, MultiBindings& mb) {
  b.clear(); mb.clear();
  return match_node(g, g.root, p, b, mb);
}

} // namespace et
