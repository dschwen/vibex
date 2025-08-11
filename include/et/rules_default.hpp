#pragma once
#include <vector>
#include "et/pattern.hpp"
#include "et/rewrite.hpp"

namespace et {

inline std::vector<Rule> default_rules() {
  using namespace et::pat;
  std::vector<Rule> rs;

  // sin(p)^2 + cos(p)^2 -> 1
  rs.push_back(Rule{
    /*lhs=*/ (sin(P(1))*sin(P(1))) + (cos(P(1))*cos(P(1))),
    /*rhs=*/ C(1.0),
    /*guard=*/ {},
    /*name=*/ "pythagorean", /*priority=*/ 10
  });

  // log(exp(p)) -> p
  rs.push_back(Rule{
    /*lhs=*/ log(exp(P(1))),
    /*rhs=*/ P(1),
    /*guard=*/ {},
    /*name=*/ "log_exp", /*priority=*/ 5
  });

  // exp(log(p)) -> p (no guard here; users may prefer to guard by domain)
  rs.push_back(Rule{
    /*lhs=*/ exp(log(P(1))),
    /*rhs=*/ P(1),
    /*guard=*/ {},
    /*name=*/ "exp_log", /*priority=*/ 5
  });

  // Like-term merging (basic): (k1*x) + (k2*x) -> (k1+k2)*x where k1,k2 const
  rs.push_back(Rule{
    /*lhs=*/ ( (pat::mul(P(2), P(1))) + (pat::mul(P(3), P(1))) ),
    /*rhs=*/ pat::mul( pat::add(P(2), P(3)), P(1) ),
    /*guard=*/ [](const RGraph& g, const Bindings& b){
      auto is_const = [&](int pid){ auto it=b.find(pid); return it!=b.end() && g.nodes[it->second].kind==NodeKind::Const; };
      return is_const(2) && is_const(3);
    },
    /*name=*/ "like_terms_add", /*priority=*/ 3
  });

  return rs;
}

} // namespace et
