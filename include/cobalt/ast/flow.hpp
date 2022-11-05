#ifndef COBALT_AST_FLOW_HPP
#define COBALT_AST_FLOW_HPP
#include "cobalt/ast/ast.hpp"
namespace cobalt::ast {
  struct top_level_ast : ast_base {
    std::vector<AST> insts;
    top_level_ast(location loc, std::vector<AST>&& insts) : ast_base(loc), CO_INIT(insts) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<top_level_ast const*>(other)) return insts == ptr->insts; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct group_ast : ast_base {
    std::vector<AST> insts;
    group_ast(location loc, std::vector<AST>&& insts) : ast_base(loc), CO_INIT(insts) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<group_ast const*>(other)) return insts == ptr->insts; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct block_ast : ast_base {
    std::vector<AST> insts;
    block_ast(location loc, std::vector<AST>&& insts) : ast_base(loc), CO_INIT(insts) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<block_ast const*>(other)) return insts == ptr->insts; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct if_ast : ast_base {
    AST cond, if_true, if_false;
    if_ast(location loc, AST&& cond, AST&& if_true, AST&& if_false = nullptr) : ast_base(loc), CO_INIT(cond), CO_INIT(if_true), CO_INIT(if_false) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<if_ast const*>(other)) return cond == ptr->cond && if_true == ptr->if_true && if_false == ptr->if_false; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct while_ast : ast_base {
    AST cond, body;
    while_ast(location loc, AST&& cond, AST&& body) : ast_base(loc), CO_INIT(cond), CO_INIT(body) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<while_ast const*>(other)) return cond == ptr->cond && body == ptr->body; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct for_ast : ast_base {
    AST cond, body;
    sstring elem_name;
    for_ast(location loc, sstring elem_name, AST&& cond, AST&& body) : ast_base(loc), CO_INIT(cond), CO_INIT(body), elem_name(elem_name) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<while_ast const*>(other)) return cond == ptr->cond && body == ptr->body; else return false;}
    typed_value codegen(compile_context& ctx) const override;
    type_ptr type(base_context& ctx) const override;
  private:
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
}
#endif
