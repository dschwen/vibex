# Repository Guidelines

## Project Structure & Module Organization
- Source: header-only under `include/et/*.hpp` (IR, ops, simplify, backends).
- Examples: `examples/01_basic_eval.cpp` … `07_hash_cse.cpp` demonstrate features.
- Build system: `CMakeLists.txt` defines the interface library `et` and example binaries.
- Optional: TorchScript backend in `include/et/torch_jit_backend.hpp` (guarded by `ET_WITH_TORCH`).

## Build, Test, and Development Commands
- Configure: `mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release ..`
- Build all: `cmake --build .` (produces example executables).
- Run examples: `./01_basic_eval`, `./02_symbolic_diff`, etc. from `build/`.
- Torch backend: `cmake -DET_WITH_TORCH=ON ..` (requires libtorch in your CMake path).

## Coding Style & Naming Conventions
- Language: C++17; header-only; no RTTI/virtuals in the IR; prefer `constexpr` and value semantics.
- Indentation: 2 spaces; braces on the same line; early `return` preferred.
- Files: snake_case (`compile_hash_cse.hpp`); op tags: PascalCase with `Op` suffix (`AddOp`).
- Operators only participate when an ET node is involved (see `expr.hpp`). Avoid widening these constraints.
- Formatting: if available, use `clang-format` (LLVM style). No config is committed; keep changes minimal and consistent with existing headers.

## Testing Guidelines
- This repo uses example programs as smoke tests. After changes, build and run: `./01_basic_eval`, `./02_symbolic_diff`, `./03_simplify`, `./04_tape_ad`, `./06_ops_and_cse`, `./07_hash_cse`.
- When adding ops/backends, extend an existing example or add a new `examples/NN_feature.cpp` to demonstrate behavior.
- Optional: compare symbolic gradients to finite differences for sanity when changing AD rules.

## Commit & Pull Request Guidelines
- Commits: imperative mood, concise subject (<72 chars) with optional body explaining rationale and scope.
- Scope: keep changes focused (e.g., “Add ExpOp derivative”); update `USAGE.md`/`DESIGN.md` if the public API changes.
- PRs: include a clear description, affected headers, build/run output or screenshots, and any new example added. Link related issues.

## Security & Configuration Tips
- TorchScript build requires libtorch; ensure `Torch_DIR` is discoverable or set CMake hints as needed.
- The tape backend assumes `double` scalars; avoid introducing heap allocations in the IR or backends.

