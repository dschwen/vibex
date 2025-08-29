#pragma once
#include <memory>
#include <vector>
#include <cstddef>
#include <cmath>

namespace et {

// Forward declaration
struct Node;

// Lightweight handle to a shared AST node
struct Expr {
  std::shared_ptr<Node> n;
  Expr() = default;
  explicit Expr(std::shared_ptr<Node> p) : n(std::move(p)) {}
  explicit operator bool() const { return static_cast<bool>(n); }
};

// Common base class for all AST nodes
struct Node {
  virtual ~Node() = default;
  virtual double eval(const std::vector<double>& inputs) const = 0;
};

// Leaf nodes
struct ConstNode final : Node {
  double value;
  explicit ConstNode(double v) : value(v) {}
  double eval(const std::vector<double>&) const override { return value; }
};

struct VarNode final : Node {
  std::size_t index{};
  explicit VarNode(std::size_t i) : index(i) {}
  double eval(const std::vector<double>& inputs) const override { return inputs[index]; }
};

// Unary nodes
struct NegNode final : Node {
  std::shared_ptr<Node> a;
  explicit NegNode(std::shared_ptr<Node> x) : a(std::move(x)) {}
  double eval(const std::vector<double>& in) const override { return -a->eval(in); }
};

struct SinNode final : Node {
  std::shared_ptr<Node> a;
  explicit SinNode(std::shared_ptr<Node> x) : a(std::move(x)) {}
  double eval(const std::vector<double>& in) const override { using std::sin; return sin(a->eval(in)); }
};

struct CosNode final : Node {
  std::shared_ptr<Node> a;
  explicit CosNode(std::shared_ptr<Node> x) : a(std::move(x)) {}
  double eval(const std::vector<double>& in) const override { using std::cos; return cos(a->eval(in)); }
};

struct ExpNode final : Node {
  std::shared_ptr<Node> a;
  explicit ExpNode(std::shared_ptr<Node> x) : a(std::move(x)) {}
  double eval(const std::vector<double>& in) const override { using std::exp; return exp(a->eval(in)); }
};

struct LogNode final : Node {
  std::shared_ptr<Node> a;
  explicit LogNode(std::shared_ptr<Node> x) : a(std::move(x)) {}
  double eval(const std::vector<double>& in) const override { using std::log; return log(a->eval(in)); }
};

struct SqrtNode final : Node {
  std::shared_ptr<Node> a;
  explicit SqrtNode(std::shared_ptr<Node> x) : a(std::move(x)) {}
  double eval(const std::vector<double>& in) const override { using std::sqrt; return sqrt(a->eval(in)); }
};

struct TanhNode final : Node {
  std::shared_ptr<Node> a;
  explicit TanhNode(std::shared_ptr<Node> x) : a(std::move(x)) {}
  double eval(const std::vector<double>& in) const override { using std::tanh; return tanh(a->eval(in)); }
};

// Binary nodes
struct AddNode final : Node {
  std::shared_ptr<Node> a, b;
  AddNode(std::shared_ptr<Node> x, std::shared_ptr<Node> y) : a(std::move(x)), b(std::move(y)) {}
  double eval(const std::vector<double>& in) const override { return a->eval(in) + b->eval(in); }
};

struct SubNode final : Node {
  std::shared_ptr<Node> a, b;
  SubNode(std::shared_ptr<Node> x, std::shared_ptr<Node> y) : a(std::move(x)), b(std::move(y)) {}
  double eval(const std::vector<double>& in) const override { return a->eval(in) - b->eval(in); }
};

struct MulNode final : Node {
  std::shared_ptr<Node> a, b;
  MulNode(std::shared_ptr<Node> x, std::shared_ptr<Node> y) : a(std::move(x)), b(std::move(y)) {}
  double eval(const std::vector<double>& in) const override { return a->eval(in) * b->eval(in); }
};

struct DivNode final : Node {
  std::shared_ptr<Node> a, b;
  DivNode(std::shared_ptr<Node> x, std::shared_ptr<Node> y) : a(std::move(x)), b(std::move(y)) {}
  double eval(const std::vector<double>& in) const override { return a->eval(in) / b->eval(in); }
};

struct PowNode final : Node {
  std::shared_ptr<Node> a, b;
  PowNode(std::shared_ptr<Node> x, std::shared_ptr<Node> y) : a(std::move(x)), b(std::move(y)) {}
  double eval(const std::vector<double>& in) const override { using std::pow; return pow(a->eval(in), b->eval(in)); }
};

// Comparisons produce 1.0 (true) or 0.0 (false)
struct LtNode final : Node { std::shared_ptr<Node> a,b; LtNode(std::shared_ptr<Node>x,std::shared_ptr<Node>y):a(std::move(x)),b(std::move(y)){} double eval(const std::vector<double>& in) const override { return a->eval(in) <  b->eval(in) ? 1.0 : 0.0; } };
struct LeNode final : Node { std::shared_ptr<Node> a,b; LeNode(std::shared_ptr<Node>x,std::shared_ptr<Node>y):a(std::move(x)),b(std::move(y)){} double eval(const std::vector<double>& in) const override { return a->eval(in) <= b->eval(in) ? 1.0 : 0.0; } };
struct GtNode final : Node { std::shared_ptr<Node> a,b; GtNode(std::shared_ptr<Node>x,std::shared_ptr<Node>y):a(std::move(x)),b(std::move(y)){} double eval(const std::vector<double>& in) const override { return a->eval(in) >  b->eval(in) ? 1.0 : 0.0; } };
struct GeNode final : Node { std::shared_ptr<Node> a,b; GeNode(std::shared_ptr<Node>x,std::shared_ptr<Node>y):a(std::move(x)),b(std::move(y)){} double eval(const std::vector<double>& in) const override { return a->eval(in) >= b->eval(in) ? 1.0 : 0.0; } };
struct EqNode final : Node { std::shared_ptr<Node> a,b; EqNode(std::shared_ptr<Node>x,std::shared_ptr<Node>y):a(std::move(x)),b(std::move(y)){} double eval(const std::vector<double>& in) const override { return a->eval(in) == b->eval(in) ? 1.0 : 0.0; } };
struct NeNode final : Node { std::shared_ptr<Node> a,b; NeNode(std::shared_ptr<Node>x,std::shared_ptr<Node>y):a(std::move(x)),b(std::move(y)){} double eval(const std::vector<double>& in) const override { return a->eval(in) != b->eval(in) ? 1.0 : 0.0; } };

// If/Select
struct IfNode final : Node {
  std::shared_ptr<Node> c,t,e;
  IfNode(std::shared_ptr<Node> c_, std::shared_ptr<Node> t_, std::shared_ptr<Node> e_) : c(std::move(c_)), t(std::move(t_)), e(std::move(e_)) {}
  double eval(const std::vector<double>& in) const override { return (c->eval(in) != 0.0) ? t->eval(in) : e->eval(in); }
};
struct SelectNode final : Node {
  std::shared_ptr<Node> m,t,e;
  SelectNode(std::shared_ptr<Node> m_, std::shared_ptr<Node> t_, std::shared_ptr<Node> e_) : m(std::move(m_)), t(std::move(t_)), e(std::move(e_)) {}
  double eval(const std::vector<double>& in) const override { return (m->eval(in) != 0.0) ? t->eval(in) : e->eval(in); }
};

// Loop nodes (evaluation is not supported in pure AST; compile to Tape for execution)
struct IterNode final : Node {
  double eval(const std::vector<double>&) const override { return 0.0; }
};
struct StateReadNode final : Node {
  std::size_t index{};
  explicit StateReadNode(std::size_t i) : index(i) {}
  double eval(const std::vector<double>&) const override { return 0.0; }
};
struct LoopForNode final : Node {
  std::size_t K{}; // number of carried state variables
  // children: [N, init0..initK-1, next0..nextK-1]
  std::vector<std::shared_ptr<Node>> ch;
  LoopForNode(std::size_t K_, std::vector<std::shared_ptr<Node>> ch_) : K(K_), ch(std::move(ch_)) {}
  double eval(const std::vector<double>&) const override { return 0.0; }
};
struct LoopOutNode final : Node {
  std::size_t J{}; // which carried var to extract
  std::shared_ptr<Node> loop;
  LoopOutNode(std::size_t J_, std::shared_ptr<Node> l_) : J(J_), loop(std::move(l_)) {}
  double eval(const std::vector<double>&) const override { return 0.0; }
};

// Construction helpers
inline Expr lit(double v) { return Expr{std::make_shared<ConstNode>(v)}; }
inline Expr var(std::size_t i) { return Expr{std::make_shared<VarNode>(i)}; }

// Operators
inline Expr operator+(const Expr& x, const Expr& y) { return Expr{std::make_shared<AddNode>(x.n, y.n)}; }
inline Expr operator-(const Expr& x, const Expr& y) { return Expr{std::make_shared<SubNode>(x.n, y.n)}; }
inline Expr operator*(const Expr& x, const Expr& y) { return Expr{std::make_shared<MulNode>(x.n, y.n)}; }
inline Expr operator/(const Expr& x, const Expr& y) { return Expr{std::make_shared<DivNode>(x.n, y.n)}; }
inline Expr operator-(const Expr& x) { return Expr{std::make_shared<NegNode>(x.n)}; }

// Math wrappers
inline Expr sin(const Expr& x) { return Expr{std::make_shared<SinNode>(x.n)}; }
inline Expr cos(const Expr& x) { return Expr{std::make_shared<CosNode>(x.n)}; }
inline Expr exp(const Expr& x) { return Expr{std::make_shared<ExpNode>(x.n)}; }
inline Expr log(const Expr& x) { return Expr{std::make_shared<LogNode>(x.n)}; }
inline Expr sqrt(const Expr& x) { return Expr{std::make_shared<SqrtNode>(x.n)}; }
inline Expr tanh(const Expr& x) { return Expr{std::make_shared<TanhNode>(x.n)}; }
inline Expr pow(const Expr& x, const Expr& y) { return Expr{std::make_shared<PowNode>(x.n, y.n)}; }

// Comparisons
inline Expr operator<(const Expr& a, const Expr& b) { return Expr{std::make_shared<LtNode>(a.n, b.n)}; }
inline Expr operator<=(const Expr& a, const Expr& b) { return Expr{std::make_shared<LeNode>(a.n, b.n)}; }
inline Expr operator>(const Expr& a, const Expr& b) { return Expr{std::make_shared<GtNode>(a.n, b.n)}; }
inline Expr operator>=(const Expr& a, const Expr& b) { return Expr{std::make_shared<GeNode>(a.n, b.n)}; }
inline Expr operator==(const Expr& a, const Expr& b) { return Expr{std::make_shared<EqNode>(a.n, b.n)}; }
inline Expr operator!=(const Expr& a, const Expr& b) { return Expr{std::make_shared<NeNode>(a.n, b.n)}; }

// If and Select
inline Expr If(const Expr& c, const Expr& t, const Expr& e) { return Expr{std::make_shared<IfNode>(c.n, t.n, e.n)}; }
inline Expr Select(const Expr& m, const Expr& t, const Expr& e) { return Expr{std::make_shared<SelectNode>(m.n, t.n, e.n)}; }

// Loop builders
inline Expr iter() { return Expr{std::make_shared<IterNode>()}; }
inline Expr state(std::size_t i) { return Expr{std::make_shared<StateReadNode>(i)}; }
inline Expr loop_for(std::size_t K, const Expr& N, const std::vector<Expr>& inits, const std::vector<Expr>& nexts) {
  std::vector<std::shared_ptr<Node>> ch; ch.reserve(1 + inits.size() + nexts.size());
  ch.push_back(N.n);
  for (auto& e : inits) ch.push_back(e.n);
  for (auto& e : nexts) ch.push_back(e.n);
  return Expr{ std::make_shared<LoopForNode>(K, std::move(ch)) };
}
inline Expr loop_out(std::size_t J, const Expr& loop) { return Expr{ std::make_shared<LoopOutNode>(J, loop.n) }; }

// Evaluate an expression with inputs by index
inline double eval(const Expr& e, const std::vector<double>& inputs) {
  return e.n ? e.n->eval(inputs) : 0.0;
}

} // namespace et
