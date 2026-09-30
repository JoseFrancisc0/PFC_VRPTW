cmake_minimum_required(VERSION 3.10.0)
project(ALNS_VRPTW VERSION 0.1.0 LANGUAGES C CXX)

add_executable(ALNS_VRPTW 
               main.cpp 
               "VRPTW Environment/instance.cpp"
               "VRPTW Environment/solution.cpp"
               Operators/destroy_ops.cpp 
               Operators/repair_ops.cpp 
               ALNS/alns.cpp 
               ALNS/alns_qlearning.cpp
               Utils/utils.cpp)

target_compile_options(ALNS_VRPTW PRIVATE "-O3")

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
