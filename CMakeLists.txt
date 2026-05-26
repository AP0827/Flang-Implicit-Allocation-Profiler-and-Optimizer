cmake_minimum_required(VERSION 3.24)
project(FlangImplicitAllocationProfiler LANGUAGES CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FIAP_HAVE_FLANG 0)

find_package(LLVM REQUIRED CONFIG)
find_package(MLIR REQUIRED CONFIG)
find_package(Flang CONFIG QUIET)

if(Flang_FOUND OR FLANG_FOUND OR FLANG_EXPORTED_TARGETS)
  set(FIAP_HAVE_FLANG 1)
endif()

message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
if(FIAP_HAVE_FLANG)
  message(STATUS "Using FlangConfig.cmake in: ${Flang_DIR}")
else()
  message(STATUS "Flang not found; building the generic MLIR prototype mode")
endif()

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}" "${MLIR_CMAKE_DIR}")
include(TableGen)
include(AddLLVM)
include(AddMLIR)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/fiap/Config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/fiap/Config.h
  @ONLY
)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${MLIR_INCLUDE_DIRS})
if(FIAP_HAVE_FLANG)
  include_directories(${FLANG_INCLUDE_DIRS})
endif()
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
link_directories(${LLVM_BUILD_LIBRARY_DIR})

set(FIAP_SOURCES
  lib/APG.cpp
  lib/AllocationClassifier.cpp
  lib/AllocationReport.cpp
  lib/ImplicitAllocationAnalysis.cpp
  lib/ImplicitAllocationProfilerPass.cpp
  lib/OperationSemantics.cpp
  lib/Transforms/PromoteTempToStack.cpp
  lib/Transforms/ScalarizeArrayExpr.cpp
)

add_library(fiap STATIC ${FIAP_SOURCES})
target_include_directories(fiap PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/include)
target_compile_definitions(fiap PUBLIC FIAP_HAVE_FLANG=${FIAP_HAVE_FLANG})
target_compile_features(fiap PUBLIC cxx_std_17)
if(MSVC)
  target_compile_options(fiap PUBLIC /Zc:__cplusplus)
endif()

target_link_libraries(fiap
  PUBLIC
    MLIRFuncDialect
    MLIRIR
    MLIRPass
    MLIRParser
    MLIRSupport
    LLVMSupport
)

if(FIAP_HAVE_FLANG)
  foreach(flang_target IN ITEMS
    FIRDialect
    HLFIRDialect
    FIRBuilder
    FIRSupport
    FIRDialectSupport
    FortranSupport
  )
    if(TARGET ${flang_target})
      target_link_libraries(fiap PUBLIC ${flang_target})
    endif()
  endforeach()
endif()

add_executable(fiap-opt tools/fiap-opt.cpp)
target_link_libraries(fiap-opt PRIVATE fiap MLIRParser MLIRSupport LLVMSupport)

enable_testing()
foreach(fiap_case IN ITEMS
  01_array_temp
  02_function_result
  03_realloc_assignment
  04_escaping_temp
  05_elemental_temp
)
  add_test(
    NAME ${fiap_case}
    COMMAND fiap-opt
            ${CMAKE_CURRENT_SOURCE_DIR}/test/mlir_regression/${fiap_case}.mlir
            --format=json
  )
endforeach()

install(TARGETS fiap fiap-opt)
