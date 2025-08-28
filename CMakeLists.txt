cmake_minimum_required(VERSION 3.14)
project(et LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable coverage flags early so all targets pick them up
option(ET_ENABLE_COVERAGE "Enable GCov/LCov coverage flags and target" OFF)
if(ET_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Enabling coverage flags (early)")
    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
  else()
    message(WARNING "Coverage requested but unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
  endif()
endif()

option(ET_WITH_TORCH "Enable TorchScript integrations" OFF)
option(ET_BUILD_TORCH_EXAMPLES "Build Torch-based example binaries" OFF)
option(ET_BUILD_TORCH_TESTS "Build Torch-based tests" OFF)
option(ET_BUILD_CONTROL_FLOW_EXAMPLE "Build control-flow example" ON)
option(ET_BUILD_CONTROL_FLOW_TESTS "Build control-flow tests" OFF)
option(ET_ENABLE_CONTROL_FLOW "Enable control-flow operators (If/Select, comparisons)" ON)
option(ET_BUILD_COMPILE_RUNTIME_LOOPS_FALLBACK_TEST "Build the compile_runtime loops fallback test" OFF)
option(ET_ENABLE_LOOP_SYMBOLIC_AD "Enable compile-time loop AD (experimental)" OFF)

# Increase maximum template instantiation depth
add_compile_options(-ftemplate-depth=12000)

add_library(et INTERFACE)
target_include_directories(et INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(ET_ENABLE_CONTROL_FLOW)
  target_compile_definitions(et INTERFACE ET_ENABLE_CONTROL_FLOW)
endif()
if(ET_ENABLE_LOOP_SYMBOLIC_AD)
  target_compile_definitions(et INTERFACE ET_ENABLE_LOOP_SYMBOLIC_AD)
endif()
## Do not define ET_WITH_TORCH globally; only enable on torch targets

add_executable(01_basic_eval examples/01_basic_eval.cpp)
target_link_libraries(01_basic_eval PRIVATE et)

add_executable(02_symbolic_diff examples/02_symbolic_diff.cpp)
target_link_libraries(02_symbolic_diff PRIVATE et)

add_executable(03_simplify examples/03_simplify.cpp)
target_link_libraries(03_simplify PRIVATE et)

add_executable(04_tape_ad examples/04_tape_ad.cpp)
target_link_libraries(04_tape_ad PRIVATE et)

add_executable(06_ops_and_cse examples/06_ops_and_cse.cpp)
target_link_libraries(06_ops_and_cse PRIVATE et)

add_executable(07_hash_cse examples/07_hash_cse.cpp)
target_link_libraries(07_hash_cse PRIVATE et)

add_executable(08_rewrite_rules examples/08_rewrite_rules.cpp)
target_link_libraries(08_rewrite_rules PRIVATE et)

add_executable(09_rewrite_nested examples/09_rewrite_nested.cpp)
target_link_libraries(09_rewrite_nested PRIVATE et)

if(ET_BUILD_CONTROL_FLOW_EXAMPLE)
  add_executable(10_control_flow examples/10_control_flow.cpp)
  target_link_libraries(10_control_flow PRIVATE et)
endif()

# ForN loop example (single-carried state)
add_executable(12_loop_forn examples/12_loop_forn.cpp)
target_link_libraries(12_loop_forn PRIVATE et)

add_executable(13_loop_fib examples/13_loop_fib.cpp)
target_link_libraries(13_loop_fib PRIVATE et)

if(ET_WITH_TORCH)
  find_package(Torch REQUIRED)

  # Ensure the executable can find libtorch's shared libraries at runtime (macOS)
  if(APPLE)
    # Derive lib dir from Torch_DIR when TORCH_INSTALL_PREFIX is not defined
    if(NOT DEFINED TORCH_INSTALL_PREFIX)
      get_filename_component(_torch_cmake_dir "${Torch_DIR}" DIRECTORY)     # .../lib/cmake
      get_filename_component(_torch_lib_dir "${_torch_cmake_dir}" DIRECTORY) # .../lib
      set(_torch_rpath "${_torch_lib_dir}")
    else()
      set(_torch_rpath "${TORCH_INSTALL_PREFIX}/lib")
    endif()
    set(CMAKE_BUILD_RPATH "${_torch_rpath};${CMAKE_BUILD_RPATH}")
    set(CMAKE_INSTALL_RPATH "${_torch_rpath};${CMAKE_INSTALL_RPATH}")
  endif()
  # Choose a single Torch link handle
  if(TARGET Torch::Torch)
    set(ET_TORCH_LIB Torch::Torch)
  else()
    set(ET_TORCH_LIB ${TORCH_LIBRARIES})
  endif()

  add_executable(05_torchjit examples/05_torchjit.cpp)
  target_link_libraries(05_torchjit PRIVATE et ${ET_TORCH_LIB})
  target_compile_definitions(05_torchjit PRIVATE ET_WITH_TORCH)
  target_compile_features(05_torchjit PRIVATE cxx_std_17)

  # Enable Torch features inside this example when requested
  target_link_libraries(06_ops_and_cse PRIVATE ${ET_TORCH_LIB})
  target_compile_definitions(06_ops_and_cse PRIVATE ET_WITH_TORCH)

  # Control-flow Torch example (Select + comparisons)
  add_executable(11_torch_control_flow examples/11_torch_control_flow.cpp)
  target_link_libraries(11_torch_control_flow PRIVATE et ${ET_TORCH_LIB})
  target_compile_definitions(11_torch_control_flow PRIVATE ET_WITH_TORCH ET_TORCH_ENABLE_MODULE_WRAPPER ET_TORCH_ENABLE_DEFINE_WRAPPER)
  target_compile_features(11_torch_control_flow PRIVATE cxx_std_17)

  # Enable Torch features in example 13 (loop + Torch graph printing)
  if(TARGET 13_loop_fib)
    target_link_libraries(13_loop_fib PRIVATE ${ET_TORCH_LIB})
    target_compile_definitions(13_loop_fib PRIVATE ET_WITH_TORCH ET_TORCH_ENABLE_MODULE_WRAPPER ET_TORCH_ENABLE_DEFINE_WRAPPER)
  endif()
endif()

## Torch example already built above; no need to duplicate under tests

# ------------------------
# Tests (assert-based + CTest)
# ------------------------
include(CTest)
if(BUILD_TESTING)
  add_executable(et_tests_core tests/test_core.cpp)
  target_link_libraries(et_tests_core PRIVATE et)
  add_test(NAME et_core COMMAND et_tests_core)

  add_executable(et_tests_ops tests/test_ops.cpp)
  target_link_libraries(et_tests_ops PRIVATE et)
  add_test(NAME et_ops COMMAND et_tests_ops)

  add_executable(et_tests_binary tests/test_binary.cpp)
  target_link_libraries(et_tests_binary PRIVATE et)
  add_test(NAME et_binary COMMAND et_tests_binary)

  add_executable(et_tests_simplify tests/test_simplify.cpp)
  target_link_libraries(et_tests_simplify PRIVATE et)
  add_test(NAME et_simplify COMMAND et_tests_simplify)

  add_executable(et_tests_cse tests/test_cse.cpp)
  target_link_libraries(et_tests_cse PRIVATE et)
  add_test(NAME et_cse COMMAND et_tests_cse)

  # New tests: runtime AST and normalization
  add_executable(et_tests_runtime tests/test_runtime.cpp)
  target_link_libraries(et_tests_runtime PRIVATE et)
  add_test(NAME et_runtime COMMAND et_tests_runtime)

  add_executable(et_tests_normalize tests/test_normalize.cpp)
  target_link_libraries(et_tests_normalize PRIVATE et)
  add_test(NAME et_normalize COMMAND et_tests_normalize)

  add_executable(et_tests_pattern tests/test_pattern_match.cpp)
  target_link_libraries(et_tests_pattern PRIVATE et)
  add_test(NAME et_pattern COMMAND et_tests_pattern)

  add_executable(et_tests_rewrite tests/test_rewrite.cpp)
  target_link_libraries(et_tests_rewrite PRIVATE et)
  add_test(NAME et_rewrite COMMAND et_tests_rewrite)

  add_executable(et_tests_rewrite_large tests/test_rewrite_large.cpp)
  target_link_libraries(et_tests_rewrite_large PRIVATE et)
  add_test(NAME et_rewrite_large COMMAND et_tests_rewrite_large)

  add_executable(et_tests_rewrite_nested_large tests/test_rewrite_nested_large.cpp)
  target_link_libraries(et_tests_rewrite_nested_large PRIVATE et)
  add_test(NAME et_rewrite_nested_large COMMAND et_tests_rewrite_nested_large)

  add_executable(et_tests_poly_factor tests/test_poly_factor.cpp)
  target_link_libraries(et_tests_poly_factor PRIVATE et)
  add_test(NAME et_poly_factor COMMAND et_tests_poly_factor)

  add_executable(et_tests_compile_runtime_tape tests/test_compile_runtime_and_tape.cpp)
  target_link_libraries(et_tests_compile_runtime_tape PRIVATE et)
  add_test(NAME et_compile_runtime_tape COMMAND et_tests_compile_runtime_tape)

  add_executable(et_tests_compile_hash_cse tests/test_compile_hash_cse.cpp)
  target_link_libraries(et_tests_compile_hash_cse PRIVATE et)
  add_test(NAME et_compile_hash_cse COMMAND et_tests_compile_hash_cse)

  # Extra loop coverage tests
  add_executable(et_tests_runtime_to_et_loops tests/test_runtime_to_et_loops.cpp)
  target_link_libraries(et_tests_runtime_to_et_loops PRIVATE et)
  add_test(NAME et_runtime_to_et_loops COMMAND et_tests_runtime_to_et_loops)

  add_executable(et_tests_compile_hash_cse_loops tests/test_compile_hash_cse_loops.cpp)
  target_link_libraries(et_tests_compile_hash_cse_loops PRIVATE et)
  add_test(NAME et_compile_hash_cse_loops COMMAND et_tests_compile_hash_cse_loops)

  if(ET_BUILD_COMPILE_RUNTIME_LOOPS_FALLBACK_TEST)
    add_executable(et_tests_compile_runtime_loops_fallback tests/test_compile_runtime_loops_fallback.cpp)
    target_link_libraries(et_tests_compile_runtime_loops_fallback PRIVATE et)
    add_test(NAME et_compile_runtime_loops_fallback COMMAND et_tests_compile_runtime_loops_fallback)
  endif()

  add_executable(et_tests_compile_runtime_var_indices tests/test_compile_runtime_var_indices.cpp)
  target_link_libraries(et_tests_compile_runtime_var_indices PRIVATE et)
  add_test(NAME et_compile_runtime_var_indices COMMAND et_tests_compile_runtime_var_indices)

  add_executable(et_tests_match_edgecases tests/test_match_edgecases.cpp)
  target_link_libraries(et_tests_match_edgecases PRIVATE et)
  add_test(NAME et_match_edgecases COMMAND et_tests_match_edgecases)

  add_executable(et_tests_rules_guards tests/test_rules_guards.cpp)
  target_link_libraries(et_tests_rules_guards PRIVATE et)
  add_test(NAME et_rules_guards COMMAND et_tests_rules_guards)

  add_executable(et_tests_normalize_sub tests/test_normalize_sub.cpp)
  target_link_libraries(et_tests_normalize_sub PRIVATE et)
  add_test(NAME et_normalize_sub COMMAND et_tests_normalize_sub)

  add_executable(et_tests_normalize_edges tests/test_normalize_edges.cpp)
  target_link_libraries(et_tests_normalize_edges PRIVATE et)
  add_test(NAME et_normalize_edges COMMAND et_tests_normalize_edges)

  add_executable(et_tests_denormalize_multi_sub tests/test_denormalize_multi_sub.cpp)
  target_link_libraries(et_tests_denormalize_multi_sub PRIVATE et)
  add_test(NAME et_denormalize_multi_sub COMMAND et_tests_denormalize_multi_sub)

  # New ForN loop test
  add_executable(et_tests_loop_forn tests/test_loop_forn.cpp)
  target_link_libraries(et_tests_loop_forn PRIVATE et)
  add_test(NAME et_loop_forn COMMAND et_tests_loop_forn)

  add_executable(et_tests_loop_forn_multi tests/test_loop_forn_multi.cpp)
  target_link_libraries(et_tests_loop_forn_multi PRIVATE et)
  add_test(NAME et_loop_forn_multi COMMAND et_tests_loop_forn_multi)

  # Tape VJP through loops (limited to init-state adjoints)
  add_executable(et_tests_tape_loop_vjp tests/test_tape_loop_vjp.cpp)
  target_link_libraries(et_tests_tape_loop_vjp PRIVATE et)
  add_test(NAME et_tape_loop_vjp COMMAND et_tests_tape_loop_vjp)

  # Symbolic loop gradients vs simple expectations
  add_executable(et_tests_symbolic_loop_grad tests/test_symbolic_loop_grad.cpp)
  target_link_libraries(et_tests_symbolic_loop_grad PRIVATE et)
  if(ET_ENABLE_LOOP_SYMBOLIC_AD)
    add_test(NAME et_symbolic_loop_grad COMMAND et_tests_symbolic_loop_grad)
  else()
    message(STATUS "Skipping loop symbolic AD test: et_symbolic_loop_grad (set ET_ENABLE_LOOP_SYMBOLIC_AD=ON to enable)")
  endif()

  add_executable(et_tests_symbolic_vs_tape_loop tests/test_symbolic_vs_tape_loop.cpp)
  target_link_libraries(et_tests_symbolic_vs_tape_loop PRIVATE et)
  if(ET_ENABLE_LOOP_SYMBOLIC_AD)
    add_test(NAME et_symbolic_vs_tape_loop COMMAND et_tests_symbolic_vs_tape_loop)
  else()
    message(STATUS "Skipping loop symbolic AD test: et_symbolic_vs_tape_loop (set ET_ENABLE_LOOP_SYMBOLIC_AD=ON to enable)")
  endif()

  add_executable(et_tests_symbolic_vs_tape_fib tests/test_symbolic_vs_tape_fib.cpp)
  target_link_libraries(et_tests_symbolic_vs_tape_fib PRIVATE et)
  if(ET_ENABLE_LOOP_SYMBOLIC_AD)
    add_test(NAME et_symbolic_vs_tape_fib COMMAND et_tests_symbolic_vs_tape_fib)
  else()
    message(STATUS "Skipping loop symbolic AD test: et_symbolic_vs_tape_fib (set ET_ENABLE_LOOP_SYMBOLIC_AD=ON to enable)")
  endif()

  if(ET_BUILD_CONTROL_FLOW_TESTS)
    add_executable(et_tests_control_flow tests/test_control_flow.cpp)
    target_link_libraries(et_tests_control_flow PRIVATE et)
    target_compile_definitions(et_tests_control_flow PRIVATE ET_ENABLE_CONTROL_FLOW)
    add_test(NAME et_control_flow COMMAND et_tests_control_flow)
  endif()

  # Control flow coverage: If/Select eval + gradients
  add_executable(et_tests_if_select_eval tests/test_if_select_eval.cpp)
  target_link_libraries(et_tests_if_select_eval PRIVATE et)
  add_test(NAME et_if_select_eval COMMAND et_tests_if_select_eval)

  add_executable(et_tests_if_select_grad tests/test_if_select_grad.cpp)
  target_link_libraries(et_tests_if_select_grad PRIVATE et)
  add_test(NAME et_if_select_grad COMMAND et_tests_if_select_grad)

  # Loop body variant with If
  add_executable(et_tests_loop_body_if tests/test_loop_body_if.cpp)
  target_link_libraries(et_tests_loop_body_if PRIVATE et)
  add_test(NAME et_loop_body_if COMMAND et_tests_loop_body_if)

if(ET_WITH_TORCH AND ET_BUILD_TORCH_TESTS)
  find_package(Torch REQUIRED)
  if(APPLE)
    if(NOT DEFINED TORCH_INSTALL_PREFIX)
      get_filename_component(_torch_cmake_dir "${Torch_DIR}" DIRECTORY)
      get_filename_component(_torch_lib_dir "${_torch_cmake_dir}" DIRECTORY)
      set(_torch_rpath "${_torch_lib_dir}")
    else()
      set(_torch_rpath "${TORCH_INSTALL_PREFIX}/lib")
    endif()
    set(CMAKE_BUILD_RPATH "${_torch_rpath};${CMAKE_BUILD_RPATH}")
    set(CMAKE_INSTALL_RPATH "${_torch_rpath};${CMAKE_INSTALL_RPATH}")
  endif()
  add_executable(et_torch_tests tests/test_torch.cpp)
  if(TARGET Torch::Torch)
    target_link_libraries(et_torch_tests PRIVATE et Torch::Torch)
  else()
    target_link_libraries(et_torch_tests PRIVATE et ${TORCH_LIBRARIES})
  endif()
  target_compile_definitions(et_torch_tests PRIVATE ET_WITH_TORCH)
  target_compile_features(et_torch_tests PRIVATE cxx_std_17)
  add_test(NAME et_torch COMMAND et_torch_tests)

  # Control-flow Torch-gated tests
  add_executable(et_torch_cf_tests tests/test_torch_control_flow.cpp)
  if(TARGET Torch::Torch)
    target_link_libraries(et_torch_cf_tests PRIVATE et Torch::Torch)
  else()
    target_link_libraries(et_torch_cf_tests PRIVATE et ${TORCH_LIBRARIES})
  endif()
  target_compile_definitions(et_torch_cf_tests PRIVATE ET_WITH_TORCH ET_ENABLE_CONTROL_FLOW)
  target_compile_features(et_torch_cf_tests PRIVATE cxx_std_17)
  add_test(NAME et_torch_cf COMMAND et_torch_cf_tests)

  add_executable(et_torch_select_exec tests/test_torch_select_exec.cpp)
  if(TARGET Torch::Torch)
    target_link_libraries(et_torch_select_exec PRIVATE et Torch::Torch)
  else()
    target_link_libraries(et_torch_select_exec PRIVATE et ${TORCH_LIBRARIES})
  endif()
  target_compile_definitions(et_torch_select_exec PRIVATE ET_WITH_TORCH ET_ENABLE_CONTROL_FLOW)
  target_compile_features(et_torch_select_exec PRIVATE cxx_std_17)
  add_test(NAME et_torch_select_exec COMMAND et_torch_select_exec)

  add_executable(et_torch_loop tests/test_torch_loop.cpp)
  if(TARGET Torch::Torch)
    target_link_libraries(et_torch_loop PRIVATE et Torch::Torch)
  else()
    target_link_libraries(et_torch_loop PRIVATE et ${TORCH_LIBRARIES})
  endif()
  target_compile_definitions(et_torch_loop PRIVATE ET_WITH_TORCH ET_ENABLE_CONTROL_FLOW)
  target_compile_features(et_torch_loop PRIVATE cxx_std_17)
  add_test(NAME et_torch_loop COMMAND et_torch_loop)
endif()
endif()

# ------------------------
# Coverage target (gcovr)
# ------------------------
if(ET_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Enabling coverage flags")
    add_compile_options(-O0 -g --coverage)
    add_link_options(--coverage)
  else()
    message(WARNING "Coverage requested but unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
  endif()

  # Try native gcovr binary first; fallback to `python3 -m gcovr` if available
  find_program(GCOVR_BIN gcovr)
  set(GCOVR_CMD)
  if(GCOVR_BIN)
    set(GCOVR_CMD ${GCOVR_BIN})
  else()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
      set(GCOVR_CMD ${Python3_EXECUTABLE} -m gcovr)
    endif()
  endif()

  if(GCOVR_CMD)
    message(STATUS "Using gcovr command: ${GCOVR_CMD}")
    add_custom_target(coverage
      # Run unit tests
      COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -V
      # Also run examples to exercise additional code paths
      COMMAND $<TARGET_FILE:01_basic_eval>
      COMMAND $<TARGET_FILE:02_symbolic_diff>
      COMMAND $<TARGET_FILE:03_simplify>
      COMMAND $<TARGET_FILE:04_tape_ad>
      COMMAND $<TARGET_FILE:06_ops_and_cse>
      COMMAND $<TARGET_FILE:07_hash_cse>
      COMMAND $<TARGET_FILE:08_rewrite_rules>
      COMMAND $<TARGET_FILE:09_rewrite_nested>
      COMMAND ${CMAKE_COMMAND} -E make_directory coverage
      COMMAND ${GCOVR_CMD} -r ${CMAKE_SOURCE_DIR} --object-directory ${CMAKE_BINARY_DIR}
              --exclude '.*tests/.*' --exclude '.*build.*/_deps/.*'
              --gcov-exclude-directories '.*/build/.*' --gcov-exclude-directories '.*/build_rename/.*'
              --print-summary --xml coverage/coverage.xml
              --html-details coverage/index.html
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Running tests/examples and generating coverage report with gcovr"
      DEPENDS
        01_basic_eval 02_symbolic_diff 03_simplify 04_tape_ad
        06_ops_and_cse 07_hash_cse 08_rewrite_rules 09_rewrite_nested
        et_tests_core et_tests_ops et_tests_binary et_tests_simplify
        et_tests_cse et_tests_runtime et_tests_normalize et_tests_pattern
        et_tests_rewrite et_tests_rewrite_large et_tests_rewrite_nested_large
        et_tests_poly_factor et_tests_compile_runtime_tape et_tests_compile_hash_cse
        et_tests_compile_runtime_var_indices et_tests_match_edgecases et_tests_rules_guards
        et_tests_normalize_sub et_tests_normalize_edges et_tests_denormalize_multi_sub
    )
  else()
    add_custom_target(coverage
      COMMAND ${CMAKE_COMMAND} -E echo "gcovr not found. Re-run CMake after installing (apt-get install gcovr OR pip install gcovr)."
      COMMENT "gcovr not available"
    )
  endif()
endif()
