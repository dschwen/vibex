# Repository Guidelines

## Project Structure & Module Organization
- Source: header-only under `include/et/*.hpp` (IR, ops, simplify, backends, CSE visitors).
- Examples: `examples/01_basic_eval.cpp` … `07_hash_cse.cpp` as runnable demos.
- Tests: assert-based unit tests in `tests/` (core, ops, simplify, CSE).

## Build, Test, and Development Commands
- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON`
- Build: `cmake --build build` (produces examples and test binaries).
- Run tests: `ctest --test-dir build -V`.
- Run examples: from `build/`, e.g., `./01_basic_eval`.
- Torch options: `-DET_WITH_TORCH=ON` to enable Torch code; gate examples/tests with `-DET_BUILD_TORCH_EXAMPLES=ON` and `-DET_BUILD_TORCH_TESTS=ON` (require `Torch_DIR`).

## Coding Style & Naming Conventions
- C++17; header-only; no RTTI/virtuals; prefer `constexpr` and value semantics.
- Indent 2 spaces; same-line braces; early returns; minimal headers.
- Files: snake_case (`compile_cse.hpp`); op tags: PascalCase `*Op` (`AddOp`).
- Operators are constrained to ET nodes (see `expr.hpp`); do not broaden SFINAE.

## Testing & Coverage
- Unit tests cover evaluation, AD (with finite differences), simplify constant folding, and structural CSE.
- Coverage: enable with `-DET_ENABLE_COVERAGE=ON` at configure time, then `cmake --build build --target coverage`.
- Reports: `build/coverage/index.html` (HTML) and `coverage.xml` (XML). If gcovr isn’t detected, re-run configure after installing or use `python3 -m gcovr`.

## Commit & Pull Request Guidelines
- Commits: imperative subject (<72 chars) with focused scope; update `README.md`/`USAGE.md`/`DESIGN.md` when public API changes.
- PRs: include description, affected headers, and test output; ensure `ctest` passes locally. CI runs tests and uploads coverage; PRs get Codecov annotations (add `CODECOV_TOKEN` for private repos).

## Security & Configuration Tips
- Torch builds require libtorch; ensure `Torch_DIR` is set or discoverable.
- Tape backend uses `double`; avoid heap allocations in IR/backends.
