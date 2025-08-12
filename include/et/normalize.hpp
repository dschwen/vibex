#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <functional>

#include "et/runtime_ast.hpp"

namespace et {

// Simple deterministic structural hash for RGraph subtrees
inline std::uint64_t r_hash(const RGraph& g, int id) {
  const RNode& n = g.nodes[id];
  auto mix = [](std::uint64_t h, std::uint64_t x){
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33; return h ^ (x + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2));
  };
  std::uint64_t h = 1469598103934665603ULL;
  h = mix(h, static_cast<std::uint64_t>(n.kind));
  switch (n.kind) {
    case NodeKind::Const: {
      union { double d; std::uint64_t u; } u { n.cval };
      h = mix(h, u.u);
      break;
    }
    case NodeKind::Var:
      h = mix(h, static_cast<std::uint64_t>(n.var_index));
      break;
    default:
      for (int cid : n.ch) h = mix(h, r_hash(g, cid));
      break;
  }
  return h;
}

struct ChildKey {
  int id;
  NodeKind kind;
  std::uint64_t h;
};

inline bool child_less(const ChildKey& a, const ChildKey& b) {
  auto rank = [](NodeKind k) {
    switch (k) {
      case NodeKind::Const: return 0;
      case NodeKind::Var:   return 1;
      default:              return 2 + static_cast<int>(k);
    }
  };
  int ra = rank(a.kind);
  int rb = rank(b.kind);
  if (ra != rb) return ra < rb;
  if (a.h != b.h) return a.h < b.h;
  return a.id < b.id;
}

// Normalize recursively: returns a new graph with canonical Add/Mul nodes
inline RGraph normalize(const RGraph& src) {
  RGraph dst;
  dst.nodes.reserve(src.nodes.size());

  // Memoized DFS
  std::vector<int> memo(src.nodes.size(), -1);
  std::function<int(int)> norm = [&](int id) -> int {
    if (memo[id] != -1) return memo[id];
    const RNode& n = src.nodes[id];
    auto add_node = [&](RNode nn){ return memo[id] = dst.add(std::move(nn)); };

    // Leaves
    if (n.kind == NodeKind::Const) {
      RNode nn; nn.kind = NodeKind::Const; nn.cval = n.cval; return add_node(std::move(nn));
    }
    if (n.kind == NodeKind::Var) {
      RNode nn; nn.kind = NodeKind::Var; nn.var_index = n.var_index; return add_node(std::move(nn));
    }

    // Recurse children first
    std::vector<int> ch; ch.reserve(n.ch.size());
    for (int cid : n.ch) ch.push_back(norm(cid));

    auto build_variadic = [&](NodeKind kind, const std::vector<int>& flat) {
      RNode nn; nn.kind = kind; nn.ch = flat; return nn; };

    if (n.kind == NodeKind::Add) {
      std::vector<int> flat; flat.reserve(ch.size());
      double csum = 0.0;
      for (int cid : ch) {
        const RNode& c = dst.nodes[cid];
        if (c.kind == NodeKind::Add) {
          for (int gcid : c.ch) flat.push_back(gcid);
        } else if (c.kind == NodeKind::Const) {
          csum += c.cval;
        } else {
          flat.push_back(cid);
        }
      }
      // Add constant if non-zero
      if (csum != 0.0) {
        RNode cn; cn.kind = NodeKind::Const; cn.cval = csum; flat.push_back(dst.add(std::move(cn)));
      }
      // Remove zeros that may have been introduced elsewhere: already handled
      if (flat.empty()) {
        RNode z; z.kind = NodeKind::Const; z.cval = 0.0; return add_node(std::move(z));
      }
      if (flat.size() == 1) return memo[id] = flat[0];
      std::vector<ChildKey> keys; keys.reserve(flat.size());
      for (int fid : flat) keys.push_back(ChildKey{fid, dst.nodes[fid].kind, r_hash(dst, fid)});
      std::sort(keys.begin(), keys.end(), child_less);
      std::vector<int> sorted; sorted.reserve(keys.size());
      for (auto& k : keys) sorted.push_back(k.id);
      return add_node(build_variadic(NodeKind::Add, sorted));
    }

    if (n.kind == NodeKind::Mul) {
      std::vector<int> flat; flat.reserve(ch.size());
      double cprod = 1.0;
      for (int cid : ch) {
        const RNode& c = dst.nodes[cid];
        if (c.kind == NodeKind::Mul) {
          for (int gcid : c.ch) flat.push_back(gcid);
        } else if (c.kind == NodeKind::Const) {
          if (c.cval == 0.0) { // annihilator
            RNode z; z.kind = NodeKind::Const; z.cval = 0.0; return add_node(std::move(z));
          }
          cprod *= c.cval;
        } else {
          flat.push_back(cid);
        }
      }
      if (cprod != 1.0) {
        RNode cn; cn.kind = NodeKind::Const; cn.cval = cprod; flat.push_back(dst.add(std::move(cn)));
      }
      // Drop multiplicative identity 1 when other children exist
      // (no explicit 1s present except via cprod, handled above)
      if (flat.empty()) {
        RNode one; one.kind = NodeKind::Const; one.cval = 1.0; return add_node(std::move(one));
      }
      if (flat.size() == 1) return memo[id] = flat[0];
      std::vector<ChildKey> keys; keys.reserve(flat.size());
      for (int fid : flat) keys.push_back(ChildKey{fid, dst.nodes[fid].kind, r_hash(dst, fid)});
      std::sort(keys.begin(), keys.end(), child_less);
      std::vector<int> sorted; sorted.reserve(keys.size());
      for (auto& k : keys) sorted.push_back(k.id);
      return add_node(build_variadic(NodeKind::Mul, sorted));
    }

    // Optional neutral simplifications for Sub/Div
    if (n.kind == NodeKind::Sub) {
      // Normalize subtraction into addition of a negated RHS: a - b -> Add(a, Neg(b))
      // This unifies sum-like structures for AC normalization.
      const RNode& b = dst.nodes[ch[1]];
      std::vector<int> terms; terms.reserve(2);
      terms.push_back(ch[0]);
      // Build -b with simple folding
      if (b.kind == NodeKind::Const) {
        RNode cn; cn.kind = NodeKind::Const; cn.cval = -b.cval; terms.push_back(dst.add(std::move(cn)));
      } else if (b.kind == NodeKind::Neg) {
        // a - (-x) => a + x
        terms.push_back(b.ch[0]);
      } else {
        RNode nb; nb.kind = NodeKind::Neg; nb.ch = { ch[1] }; terms.push_back(dst.add(std::move(nb)));
      }
      // Now normalize as an Add over 'terms'
      double csum = 0.0;
      std::vector<int> flat; flat.reserve(terms.size());
      for (int cid : terms) {
        const RNode& c = dst.nodes[cid];
        if (c.kind == NodeKind::Add) {
          for (int gcid : c.ch) flat.push_back(gcid);
        } else if (c.kind == NodeKind::Const) {
          csum += c.cval;
        } else {
          flat.push_back(cid);
        }
      }
      if (csum != 0.0) { RNode cn; cn.kind = NodeKind::Const; cn.cval = csum; flat.push_back(dst.add(std::move(cn))); }
      if (flat.empty()) { RNode z; z.kind = NodeKind::Const; z.cval = 0.0; return add_node(std::move(z)); }
      if (flat.size() == 1) return memo[id] = flat[0];
      std::vector<ChildKey> keys; keys.reserve(flat.size());
      for (int fid : flat) keys.push_back(ChildKey{fid, dst.nodes[fid].kind, r_hash(dst, fid)});
      std::sort(keys.begin(), keys.end(), child_less);
      std::vector<int> sorted; sorted.reserve(keys.size());
      for (auto& k : keys) sorted.push_back(k.id);
      RNode nn; nn.kind = NodeKind::Add; nn.ch = std::move(sorted); return add_node(std::move(nn));
    }
    if (n.kind == NodeKind::Div) {
      const RNode& a = dst.nodes[ch[0]];
      const RNode& b = dst.nodes[ch[1]];
      if (a.kind == NodeKind::Const && a.cval == 0.0) { RNode z; z.kind = NodeKind::Const; z.cval = 0.0; return add_node(std::move(z)); }
      if (b.kind == NodeKind::Const && b.cval == 1.0) return memo[id] = ch[0];
      if (r_equal(dst, ch[0], ch[1])) { RNode one; one.kind = NodeKind::Const; one.cval = 1.0; return add_node(std::move(one)); }
      RNode nn; nn.kind = NodeKind::Div; nn.ch = ch; return add_node(std::move(nn));
    }

    // Unary and other ops: rebuild with normalized children
    if (n.kind == NodeKind::Neg || n.kind == NodeKind::Sin || n.kind == NodeKind::Cos ||
        n.kind == NodeKind::Exp || n.kind == NodeKind::Log || n.kind == NodeKind::Sqrt || n.kind == NodeKind::Tanh) {
      if (n.kind == NodeKind::Neg) {
        const RNode& a = dst.nodes[ch[0]];
        if (a.kind == NodeKind::Const) { RNode cn; cn.kind = NodeKind::Const; cn.cval = -a.cval; return add_node(std::move(cn)); }
        if (a.kind == NodeKind::Neg)  { return memo[id] = a.ch[0]; }
      }
      RNode nn; nn.kind = n.kind; nn.ch = ch; return add_node(std::move(nn));
    }

    // Fallback
    RNode nn; nn.kind = n.kind; nn.ch = ch; return add_node(std::move(nn));
  };

  dst.root = norm(src.root);
  return dst;
}

// Optional pretty-print denormalization: turn Add(a,Neg(b)) with exactly 2 terms back into Sub(a,b).
// Also handles Add(x, Const(-k)) -> Sub(x, Const(k)) when exactly 2 terms.
inline RGraph denormalize_sub(const RGraph& src) {
  RGraph dst;
  dst.nodes.reserve(src.nodes.size());
  std::vector<int> memo(src.nodes.size(), -1);
  std::function<int(int)> rec = [&](int id) -> int {
    if (memo[id] != -1) return memo[id];
    const RNode& n = src.nodes[id];
    auto add_node = [&](RNode nn){ return memo[id] = dst.add(std::move(nn)); };

    // Leaves
    if (n.kind == NodeKind::Const) { RNode nn; nn.kind = NodeKind::Const; nn.cval = n.cval; return add_node(std::move(nn)); }
    if (n.kind == NodeKind::Var)   { RNode nn; nn.kind = NodeKind::Var; nn.var_index = n.var_index; return add_node(std::move(nn)); }

    // Recurse children
    std::vector<int> ch; ch.reserve(n.ch.size());
    for (int cid : n.ch) ch.push_back(rec(cid));

    if (n.kind == NodeKind::Add && ch.size() == 2) {
      const RNode& a = dst.nodes[ch[0]];
      const RNode& b = dst.nodes[ch[1]];
      auto make_sub = [&](int lhs_id, int rhs_id) {
        RNode nn; nn.kind = NodeKind::Sub; nn.ch = { lhs_id, rhs_id }; return add_node(std::move(nn));
      };
      // Case: second is Neg(x): Sub(first, x)
      if (b.kind == NodeKind::Neg) {
        return make_sub(ch[0], b.ch[0]);
      }
      // Case: first is Neg(x): Sub(second, x)
      if (a.kind == NodeKind::Neg) {
        return make_sub(ch[1], a.ch[0]);
      }
      // Case: one is negative constant
      if (b.kind == NodeKind::Const && b.cval < 0.0) {
        RNode cp; cp.kind = NodeKind::Const; cp.cval = -b.cval; int pid = dst.add(std::move(cp));
        return make_sub(ch[0], pid);
      }
      if (a.kind == NodeKind::Const && a.cval < 0.0) {
        RNode cp; cp.kind = NodeKind::Const; cp.cval = -a.cval; int pid = dst.add(std::move(cp));
        return make_sub(ch[1], pid);
      }
      // Otherwise leave as Add
      RNode nn; nn.kind = NodeKind::Add; nn.ch = ch; return add_node(std::move(nn));
    }

    // Generic rebuild
    RNode nn; nn.kind = n.kind; nn.ch = ch; nn.cval = n.cval; nn.var_index = n.var_index; return add_node(std::move(nn));
  };
  dst.root = rec(src.root);
  return dst;
}

} // namespace et
