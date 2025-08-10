# et: Minimal Expression Templates with Symbolic AD and Backends

This is a compact, header-only expression template library that builds an IR from C++ operator overloading, supports **symbolic automatic differentiation**, a tiny **simplifier**, and multiple **backends**:
- Eager evaluation (`evaluate`)
- **Reverse-mode tape** backend (forward + VJP gradients)
- **TorchScript/JIT** backend (optional, with `ET_WITH_TORCH`)

## Layout

```
include/et/expr.hpp            # core IR, ops, diff(), compile visitor, Vars(), grad()
include/et/simplify.hpp        # algebraic simplifier (const fold + neutral/annihilator rules)
include/et/tape_backend.hpp    # reverse-mode tape backend
include/et/torch_jit_backend.hpp  # TorchScript backend (optional; requires libtorch)
examples/01_basic_eval.cpp
examples/02_symbolic_diff.cpp
examples/03_simplify.cpp
examples/04_tape_ad.cpp
examples/05_torchjit.cpp       # optional; compile with -DET_WITH_TORCH and link libtorch
CMakeLists.txt                 # builds examples 1–4 by default
```

## Build

```bash
cmake -S . -B build
cmake --build build -j
./build/examples/01_basic_eval
./build/examples/02_symbolic_diff
./build/examples/03_simplify
./build/examples/04_tape_ad
```

### Torch example

If you have libtorch:
```bash
cmake -S . -B build -DET_WITH_TORCH=ON -DCMAKE_PREFIX_PATH=/path/to/libtorch
cmake --build build -j
./build/examples/05_torchjit
```

## API sketch

```cpp
using namespace et;

auto [x,y,z] = Vars<double,3>();     // placeholders Var<double,0/1/2>

auto f = sin(x)*y + z*z;             // build expression tree

double val = evaluate(f, 2.4, 6.0, 1.5);

auto dfdx = diff(f, x);              // symbolic derivative w.r.t x
auto dfdx_s = simplify(dfdx);        // simplified derivative

// Tape backend
TapeBackend TB(3);
int out_id = compile(f, TB);
TB.tape.output_id = out_id;
double v = TB.tape.forward({2.4,6.0,1.5});
auto grad = TB.tape.vjp({2.4,6.0,1.5}); // [df/dx, df/dy, df/dz]

// Torch JIT (optional)
#ifdef ET_WITH_TORCH
TorchJITBackend JB(3);
auto vptr = compile(f, JB);
JB.g.registerOutput(vptr);
// Wrap JB.g into a torch::jit::Function/Module as desired.
#endif
```
