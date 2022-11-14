cmake_minimum_required(VERSION 3.5)

project(Cobalt LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

execute_process(OUTPUT_VARIABLE LLVM_FLAGS COMMAND llvm-config --cppflags --ldflags --libs)
string(STRIP "${LLVM_FLAGS}" LLVM_FLAGS)
string(REPLACE "\n" ";" LLVM_FLAGS "${LLVM_FLAGS}")
list(POP_FRONT LLVM_FLAGS CPP_FLAGS LINK_DIR LLVM_LIB)
string(REPLACE " " ";" CPP_FLAGS "${CPP_FLAGS}")
string(REPLACE " " ";" LINK_DIR "${LINK_DIR}")
string(REPLACE " " ";" LLVM_LIB "${LLVM_LIB}")
add_compile_options(${CPP_FLAGS})
add_link_options(${LINK_DIR} ${LLVM_LIB})

include_directories(include)
add_library(cobalt
  include/cobalt.hpp
    include/cobalt/ast.hpp include/cobalt/ast/ast.hpp include/cobalt/ast/flow.hpp include/cobalt/ast/funcs.hpp include/cobalt/ast/literals.hpp include/cobalt/ast/scope.hpp include/cobalt/ast/vars.hpp
    include/cobalt/support/location.hpp include/cobalt/support/sstring.hpp include/cobalt/support/functions.hpp include/cobalt/support/token.hpp
    include/cobalt/types.hpp include/cobalt/types/types.hpp include/cobalt/types/numeric.hpp include/cobalt/types/pointers.hpp include/cobalt/types/structurals.hpp
    include/cobalt/context.hpp include/cobalt/varmap.hpp include/cobalt/typed_value.hpp
  src/cobalt/tokenizer.cpp src/cobalt/macros.cpp src/cobalt/parser.cpp src/cobalt/print-ast.cpp src/cobalt/ast-type.cpp src/cobalt/codegen.cpp)
if(CMAKE_BUILD_TYPE STREQUAL Debug AND EXISTS "${PROJECT_SOURCE_DIR}/stacktrace.cpp")
  target_sources(cobalt PUBLIC "${PROJECT_SOURCE_DIR}/stacktrace.cpp")
endif()
add_executable(co src/co/main.cpp)
target_link_libraries(co cobalt)

# tests
add_executable(test tests/main.cpp tests/test.hpp
  tests/tokenizer.hpp tests/parser.hpp)
target_link_libraries(test cobalt)

# build standard library
include(./cmake/StdLib.cmake)
add_stdlib(co-std PREFIX __co_ TARGETS std/c/compat.h std/c/std.h std/c/alloc.cpp std/c/file.cpp LIBS c gcc)
set_property(TARGET co-std PROPERTY OUTPUT_NAME co)

# main group to ignore tests
add_custom_target(main)
add_dependencies(main cobalt co co-std)

# installation
install(TARGETS cobalt co-std LIBRARY)
install(TARGETS co RUNTIME)
install(DIRECTORY include/ TYPE INCLUDE)
