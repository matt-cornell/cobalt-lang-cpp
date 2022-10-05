#ifndef COBALT_AST_FUNCS_HPP
#define COBALT_AST_FUNCS_HPP
#include "cobalt/ast/ast.hpp"
#include "cobalt/types/types.hpp"
namespace cobalt::ast {
  struct binop_ast : ast_base {
    sstring op;
    AST lhs, rhs;
    binop_ast(location loc, sstring op, AST&& lhs, AST&& rhs) : ast_base(loc), op(op), CO_INIT(lhs), CO_INIT(rhs) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<binop_ast const*>(other)) return op == ptr->op && lhs == ptr->lhs && rhs == ptr->rhs; else return false;}
  private: 
    typed_value codegen_impl(compile_context& ctx) const override;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct unop_ast : ast_base {
    sstring op;
    AST val;
    unop_ast(location loc, sstring op, AST&& val) : ast_base(loc), op(op), CO_INIT(val) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<unop_ast const*>(other)) return op == ptr->op && val == ptr->val; else return false;}
  private: 
    typed_value codegen_impl(compile_context& ctx) const override;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct call_ast : ast_base {
    sstring name;
    std::vector<AST> args;
    call_ast(location loc, sstring name, std::vector<AST>&& args) : ast_base(loc), name(name), CO_INIT(args) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<call_ast const*>(other)) return name == ptr->name && args == ptr->args; else return false;}
  private: 
    typed_value codegen_impl(compile_context& ctx) const override;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct fndef_ast : ast_base {
    sstring name;
    std::vector<type_ptr> args;
    fndef_ast(location loc, sstring name, std::vector<type_ptr>&& args) : ast_base(loc), name(name), CO_INIT(args) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<fndef_ast const*>(other)) return name == ptr->name && args == ptr->args; else return false;}
  private: 
    typed_value codegen_impl(compile_context& ctx) const override;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
}
#endif