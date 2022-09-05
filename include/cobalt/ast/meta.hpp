#ifndef COBALT_AST_META_HPP
#define COBALT_AST_META_HPP
#include "ast.hpp"
namespace cobalt::ast {
  struct embed_ast : ast_base {
    std::string code;
    embed_ast(std::string&& code, location loc) : ast_base(loc), code(std::move(code)) {}
    private: typed_value codegen_impl(compile_context& ctx) const override;
  };
  struct llvm_ast : ast_base {
    std::string code;
    llvm_ast(std::string&& code, location loc) : ast_base(loc), code(std::move(code)) {}
    private: typed_value codegen_impl(compile_context& ctx) const override;
  };
  struct asm_ast : ast_base {
    std::string code;
    asm_ast(std::string&& code, location loc) : ast_base(loc), code(std::move(code)) {}
    private: typed_value codegen_impl(compile_context& ctx) const override;
  };
}
#endif