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
include(CMakePackageConfigHelpers)

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
target_include_directories(fiap
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
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

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  find_program(FIAP_FLANG_EXECUTABLE
    NAMES flang-new flang flang-new.exe flang.exe
    HINTS "D:/llvm-project/build/bin"
  )

  if(Python3_Interpreter_FOUND)
    add_test(
      NAME fiap_python_helpers_compile
      COMMAND
        ${Python3_EXECUTABLE}
        -m compileall
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    set_tests_properties(fiap_python_helpers_compile PROPERTIES TIMEOUT 30)
  endif()

  if(Python3_Interpreter_FOUND AND FIAP_FLANG_EXECUTABLE)
    add_test(
      NAME fiap_real_fortran_pipeline
      COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_pipeline.py
        --tool $<TARGET_FILE:fiap-opt>
        --build-dir ${CMAKE_BINARY_DIR}
        --reports-dir ${CMAKE_BINARY_DIR}/ctest-reports
        --flang ${FIAP_FLANG_EXECUTABLE}
        --skip-benchmark
    )
    set_tests_properties(fiap_real_fortran_pipeline PROPERTIES TIMEOUT 120)

    add_test(
      NAME fiap_pipeline_evidence_checks
      COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_pipeline_outputs.py
        --reports-dir ${CMAKE_BINARY_DIR}/ctest-reports
    )
    set_tests_properties(fiap_pipeline_evidence_checks PROPERTIES
      DEPENDS fiap_real_fortran_pipeline
      TIMEOUT 30
    )

    add_test(
      NAME fiap_lit_style_regression_checks
      COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_lit_style_checks.py
        --reports-dir ${CMAKE_BINARY_DIR}/ctest-reports
    )
    set_tests_properties(fiap_lit_style_regression_checks PROPERTIES
      DEPENDS fiap_real_fortran_pipeline
      TIMEOUT 30
    )
  endif()
endif()

install(TARGETS fiap fiap-opt
  EXPORT FIAPTargets
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)
install(DIRECTORY include/fiap DESTINATION include)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/include/fiap/Config.h DESTINATION include/fiap)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/FIAPConfigVersion.cmake
  VERSION 1.0.0
  COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FIAPConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/FIAPConfig.cmake
  INSTALL_DESTINATION lib/cmake/FIAP
)
install(EXPORT FIAPTargets
  NAMESPACE FIAP::
  DESTINATION lib/cmake/FIAP
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/FIAPConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/FIAPConfigVersion.cmake
  DESTINATION lib/cmake/FIAP
)
