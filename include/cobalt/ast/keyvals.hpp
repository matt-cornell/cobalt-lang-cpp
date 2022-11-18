#ifndef COBALT_AST_KEYVALS_HPP
#define COBALT_AST_KEYVALS_HPP
#include "cobalt/ast/ast.hpp"
namespace cobalt::ast {
  struct null_ast : ast_base {
    null_ast(location loc) : ast_base(loc) {}
    bool is_const() const noexcept override {return true;}
    bool eq(ast_base const* other) const override {return typeid(other) == typeid(null_ast const*);}
    typed_value codegen(compile_context& ctx = global) const override;
    type_ptr type(base_context& ctx = global) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
}
#endif