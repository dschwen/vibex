#pragma once
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "et/runtime_ast.hpp"
#include "et/pattern.hpp"

namespace et {

using Bindings = std::unordered_map<int,int>; // pid -> node id

inline bool is_ac(NodeKind k) { return k == NodeKind::Add || k == NodeKind::Mul; }

// Forward decl
bool match_node(const RGraph& g, int id, const pat::Pattern& p, Bindings& b);

// AC multiset matching with backtracking
inline bool match_ac(const RGraph& g, const RNode& n, const pat::Pattern& p, Bindings& b) {
  if (n.kind != p.node_kind) return false;
  // Early prune: pattern cannot match if it has more children than expr
  if (p.ch.size() > n.ch.size()) return false;

  // Order pattern children by decreasing specificity
  std::vector<std::size_t> pidx(p.ch.size());
  for (std::size_t i = 0; i < pidx.size(); ++i) pidx[i] = i;
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
      auto b_snapshot = b;
      if (match_node(g, cand, pc, b)) {
        // consume cand
        int last = remaining.back(); remaining[r] = last; remaining.pop_back();
        if (dfs(i+1)) return true;
        // backtrack
        remaining.push_back(last); // restore size; position will be re-evaluated safely as we return
      }
      b = std::move(b_snapshot);
    }
    return false;
  };

  bool ok = dfs(0);
  return ok;
}

inline bool match_node(const RGraph& g, int id, const pat::Pattern& p, Bindings& b) {
  if (p.kind == pat::Pattern::Kind::Placeholder) {
    auto it = b.find(p.placeholder_id);
    if (it == b.end()) { b.emplace(p.placeholder_id, id); return true; }
    return r_equal(g, it->second, id);
  }

  const RNode& n = g.nodes[id];
  if (n.kind != p.node_kind) return false;

  if (is_ac(n.kind)) {
    return match_ac(g, n, p, b);
  }

  // Non-AC: arity must match
  if (n.ch.size() != p.ch.size()) return false;
  for (std::size_t i = 0; i < n.ch.size(); ++i) {
    if (!match_node(g, n.ch[i], p.ch[i], b)) return false;
  }
  return true;
}

inline bool match(const RGraph& g, const pat::Pattern& p, Bindings& b) {
  b.clear();
  return match_node(g, g.root, p, b);
}

} // namespace et

