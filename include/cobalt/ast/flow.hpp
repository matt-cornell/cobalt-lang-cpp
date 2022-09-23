#ifndef COBALT_AST_FLOW_HPP
#define COBALT_AST_FLOW_HPP
#include "cobalt/ast/ast.hpp"
namespace cobalt::ast {
  struct block_ast : ast_base {
    std::vector<AST> insts;
    block_ast(location loc, std::vector<AST>&& insts) : ast_base(loc), CO_INIT(insts) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<block_ast const*>(other)) return insts == ptr->insts; else return false;}
  };
  struct if_ast : ast_base {
    AST cond, if_true, if_false;
    if_ast(location loc, AST&& cond, AST&& if_true, AST&& if_false = nullptr) : ast_base(loc), CO_INIT(cond), CO_INIT(if_true), CO_INIT(if_false) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<if_ast const*>(other)) return cond == ptr->cond && if_true == ptr->if_true && if_false == ptr->if_false; else return false;}
  };
  struct while_ast : ast_base {
    AST cond, body;
    while_ast(location loc, AST&& cond, AST&& body) : ast_base(loc), CO_INIT(cond), CO_INIT(body) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<while_ast const*>(other)) return cond == ptr->cond && body == ptr->body; else return false;}
  };
  struct for_ast : ast_base {
    AST cond, body;
    sstring elem_name;
    while_ast(location loc, AST&& cond, AST&& body) : ast_base(loc), CO_INIT(cond), CO_INIT(body) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<while_ast const*>(other)) return cond == ptr->cond && body == ptr->body; else return false;}
  };
}
#endif