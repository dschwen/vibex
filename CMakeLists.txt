cmake_minimum_required(VERSION 3.14)
project(et LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ET_WITH_TORCH "Build TorchScript backend example" OFF)

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

if(ET_WITH_TORCH)
  find_package(Torch REQUIRED)
  add_executable(05_torchjit examples/05_torchjit.cpp)
  target_link_libraries(05_torchjit PRIVATE et ${TORCH_LIBRARIES})
  target_compile_definitions(05_torchjit PRIVATE ET_WITH_TORCH)
  target_compile_features(05_torchjit PRIVATE cxx_std_14)
endif()


add_executable(06_ops_and_cse examples/06_ops_and_cse.cpp)
target_link_libraries(06_ops_and_cse PRIVATE et)
if(ET_WITH_TORCH)
  target_link_libraries(06_ops_and_cse PRIVATE ${TORCH_LIBRARIES})
  target_compile_definitions(06_ops_and_cse PRIVATE ET_WITH_TORCH)
endif()


add_executable(07_hash_cse examples/07_hash_cse.cpp)
target_link_libraries(07_hash_cse PRIVATE et)
