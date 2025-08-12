# Vibex — *Vibe-Coded Expression Templates*
*“Written on pure vibes with GPT-5”*

```
 __      __ _  _
 \ \    / /(_)| |
  \ \  / /  _ | |__   ___ __  __
   \ \/ /  | ||  _ \ / _ \\ \/ /
    \  /   | || | ) |  __/ )  (
     \/    |_||____/ \___|/_/\_\
      Vibe-Coded with GPT-5
```

Vibex is a **header-only C++ expression template engine** for building, simplifying, and compiling symbolic expressions into backend-specific execution graphs. Born from an experimental “vibe coding” session with GPT-5, Vibex combines:

- **Template-driven expression building** — fully type-safe and constexpr-friendly
- **Compile-time constant folding** to eliminate trivial work before runtime
- **Common Subexpression Elimination (CSE)** — both structural and hash-based
- **Algebraic rewrite system** — rule-based simplification over a normalized runtime AST
- **Backend-agnostic compilation** — output to tapes, evaluators, or your own executor
- **Operator overloading limited to ET nodes** — no pollution of unrelated types

Whether you’re building a symbolic math library, optimizing DSLs, or just want to see how far you can push C++ metaprogramming without losing your mind, Vibex keeps the vibes high and the compile times… reasonable.

See `DESIGN_REWRITE.md` for details on the rewrite engine and `examples/08_rewrite_rules.cpp` for a live demo.
