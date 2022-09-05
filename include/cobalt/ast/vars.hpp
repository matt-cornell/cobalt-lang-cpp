#ifndef COBALT_AST_VARS_HPP
#define COBALT_AST_VARS_HPP
#include "cobalt/ast/ast.hpp"
namespace cobalt {
  namespace ast {
    struct vardef_ast : ast_base {
      typed_value codegen(compile_context& ctx) const override;
    };
    struct mutdef_ast : ast_base {
      typed_value codegen(compile_context& ctx) const override;
    };
    struct varget_ast : ast_base {
      typed_value codegen(compile_context& ctx) const override;
    };
  }
}
#endif