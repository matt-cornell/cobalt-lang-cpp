#ifndef COBALT_AST_STRUCT_HPP
#define COBALT_AST_STRUCT_HPP
#include "cobalt/ast/ast.hpp"
namespace cobalt::ast {
  struct struct_ast : ast_base {
    std::vector<sstring, sstring> fields;
  };
}
#endif