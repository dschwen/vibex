#pragma once
#include "et/rewrite.hpp"
#include "et/rules_default.hpp"

namespace et {

// Overload: optimize using the default rule set
template <class Expr>
inline RGraph optimize(const Expr& e, int max_passes = 6) {
  return optimize(e, default_rules(), max_passes);
}

} // namespace et

