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
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct unop_ast : ast_base {
    sstring op;
    AST val;
    unop_ast(location loc, sstring op, AST&& val) : ast_base(loc), op(op), CO_INIT(val) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<unop_ast const*>(other)) return op == ptr->op && val == ptr->val; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct cast_ast : ast_base {
    sstring target;
    AST val;
    cast_ast(location loc, sstring target, AST val) : ast_base(loc), target(target), CO_INIT(val) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<cast_ast const*>(other)) return target == ptr->target && val == ptr->val; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct call_ast : ast_base {
    AST val;
    std::vector<AST> args;
    call_ast(location loc, AST val, std::vector<AST>&& args) : ast_base(loc), CO_INIT(val), CO_INIT(args) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<call_ast const*>(other)) return val == ptr->val && args == ptr->args; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct subscr_ast : ast_base {
    AST val;
    std::vector<AST> args;
    subscr_ast(location loc, AST val, std::vector<AST>&& args) : ast_base(loc), CO_INIT(val), CO_INIT(args) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<call_ast const*>(other)) return val == ptr->val && args == ptr->args; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct fndef_ast : ast_base {
    sstring name, ret;
    std::vector<std::pair<sstring, sstring>> args;
    AST body;
    std::vector<std::string> annotations;
    fndef_ast(location loc, sstring name, sstring ret, std::vector<std::pair<sstring, sstring>>&& args, AST&& body, std::vector<std::string>&& annotations) : ast_base(loc), name(name), ret(ret), CO_INIT(args), CO_INIT(body), CO_INIT(annotations) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<fndef_ast const*>(other)) return name == ptr->name && args == ptr->args; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
}
#endif
