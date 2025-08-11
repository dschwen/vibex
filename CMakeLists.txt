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

# Increase maximum template instantiation depth
add_compile_options(-ftemplate-depth=12000)

add_library(et INTERFACE)
target_include_directories(et INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

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

if(ET_WITH_TORCH AND ET_BUILD_TORCH_EXAMPLES)
  find_package(Torch REQUIRED)
  add_executable(05_torchjit examples/05_torchjit.cpp)
  target_link_libraries(05_torchjit PRIVATE et ${TORCH_LIBRARIES})
  target_compile_definitions(05_torchjit PRIVATE ET_WITH_TORCH)
  target_compile_features(05_torchjit PRIVATE cxx_std_17)

  # Enable Torch features inside this example when requested
  target_link_libraries(06_ops_and_cse PRIVATE ${TORCH_LIBRARIES})
  target_compile_definitions(06_ops_and_cse PRIVATE ET_WITH_TORCH)
endif()

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

  if(ET_WITH_TORCH AND ET_BUILD_TORCH_TESTS)
    find_package(Torch REQUIRED)
    add_executable(et_torch_tests tests/test_torch.cpp)
    target_link_libraries(et_torch_tests PRIVATE et ${TORCH_LIBRARIES})
    target_compile_definitions(et_torch_tests PRIVATE ET_WITH_TORCH)
    target_compile_features(et_torch_tests PRIVATE cxx_std_17)
    add_test(NAME et_torch COMMAND et_torch_tests)
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
      COMMAND ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR} -V
      COMMAND ${CMAKE_COMMAND} -E make_directory coverage
      COMMAND ${GCOVR_CMD} -r ${CMAKE_SOURCE_DIR}
              --exclude '.*tests/.*' --exclude '.*examples/.*'
              --print-summary --xml coverage/coverage.xml
              --html-details coverage/index.html
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Running tests and generating coverage report with gcovr"
    )
  else()
    add_custom_target(coverage
      COMMAND ${CMAKE_COMMAND} -E echo "gcovr not found. Re-run CMake after installing (apt-get install gcovr OR pip install gcovr)."
      COMMENT "gcovr not available"
    )
  endif()
endif()
