#ifndef COBALT_AST_META_HPP
#define COBALT_AST_META_HPP
#include "cobalt/ast/ast.hpp"
namespace cobalt::ast {
  struct llvm_ast : ast_base {
    std::string code;
    llvm_ast(location loc, std::string&& code) : ast_base(loc), code(std::move(code)) {}
    private: typed_value codegen_impl(compile_context& ctx) const override;
  };
  struct asm_ast : ast_base {
    std::string code;
    asm_ast(location loc, std::string&& code) : ast_base(loc), code(std::move(code)) {}
    private: typed_value codegen_impl(compile_context& ctx) const override;
  };
}
#endif