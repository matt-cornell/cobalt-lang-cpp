cmake_minimum_required(VERSION 3.5)

project(Cobalt LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
execute_process(OUTPUT_VARIABLE LLVM_FLAGS COMMAND llvm-config --cppflags --ldflags --libs)
string(STRIP "${LLVM_FLAGS}" LLVM_FLAGS)
string(REPLACE "\n" ";" LLVM_FLAGS "${LLVM_FLAGS}")
list(POP_FRONT LLVM_FLAGS CPP_FLAGS LINK_DIR LLVM_LIB)
add_compile_options(${CPP_FLAGS})
link_directories(${LINK_DIR})
link_libraries(${LLVM_LIB})
include_directories(include)
add_library(cobalt
  include/cobalt.hpp
    include/cobalt/ast.hpp include/cobalt/ast/ast.hpp include/cobalt/ast/meta.hpp include/cobalt/ast/vars.hpp
    include/cobalt/support/location.hpp include/cobalt/support/sstring.hpp include/cobalt/support/functions.hpp include/cobalt/support/token.hpp
    include/cobalt/types/types.hpp include/cobalt/types/numeric.hpp include/cobalt/types/pointers.hpp include/cobalt/types/structurals.hpp
    include/cobalt/context.hpp include/cobalt/varmap.hpp include/cobalt/typed_value.hpp
  src/cobalt/tokenizer.cpp src/cobalt/parser.cpp)
add_executable(co src/co/main.cpp)
target_link_libraries(co cobalt)

# build standard library; will be added later
# list(APPEND CMAKE_INCLUDE_PATH "cmake/")
# include(StdLib)
# add_stdlib(co-std PREFIX __co_ TARGETS std/c/compat.h std/c/std.h std/c/alloc.cpp st/c/file.cpp LIBS c gcc)
