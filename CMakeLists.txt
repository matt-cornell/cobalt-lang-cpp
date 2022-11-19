cmake_minimum_required(VERSION 3.5)

project(Cobalt LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include_directories(include)
add_library(cobalt SHARED
  include/cobalt.hpp
    include/cobalt/ast.hpp include/cobalt/ast/ast.hpp include/cobalt/ast/flow.hpp include/cobalt/ast/funcs.hpp include/cobalt/ast/keyvals.hpp include/cobalt/ast/literals.hpp include/cobalt/ast/scope.hpp include/cobalt/ast/vars.hpp
    include/cobalt/support/location.hpp include/cobalt/support/sstring.hpp include/cobalt/support/functions.hpp include/cobalt/support/token.hpp
    include/cobalt/types.hpp include/cobalt/types/types.hpp include/cobalt/types/null.hpp include/cobalt/types/numeric.hpp include/cobalt/types/pointers.hpp include/cobalt/types/structurals.hpp include/cobalt/types/functions.hpp
    include/cobalt/context.hpp include/cobalt/varmap.hpp include/cobalt/typed_value.hpp
  src/cobalt/tokenizer.cpp src/cobalt/macros.cpp src/cobalt/parser.cpp src/cobalt/print-ast.cpp src/cobalt/ast-type.cpp src/cobalt/codegen.cpp)
if(CMAKE_BUILD_TYPE STREQUAL Debug AND EXISTS "${PROJECT_SOURCE_DIR}/stacktrace.cpp")
  target_sources(cobalt PUBLIC "${PROJECT_SOURCE_DIR}/stacktrace.cpp")
endif()
target_link_libraries(cobalt LLVM)
add_executable(co src/co/main.cpp)
target_link_libraries(co cobalt)

# tests
add_executable(test tests/main.cpp tests/test.hpp
  tests/tokenizer.hpp tests/parser.hpp)
target_link_libraries(test cobalt)

# build standard library
add_library(co-std SHARED std/c/compat.h std/c/std.h std/c/alloc.cpp std/c/file.cpp std/c/misc.cpp std/c/string.cpp)
add_custom_command(TARGET co-std POST_BUILD COMMAND ${CMAKE_STRIP} -wK __co_* $<TARGET_FILE:co-std>)
set_property(TARGET co-std PROPERTY OUTPUT_NAME co)

# main group to ignore tests
add_custom_target(main)
add_dependencies(main cobalt co co-std)

# installation
install(TARGETS cobalt co-std LIBRARY)
install(TARGETS co RUNTIME)
install(DIRECTORY include/ TYPE INCLUDE)
