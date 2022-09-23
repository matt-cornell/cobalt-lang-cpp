#ifndef COBALT_AST_FUNCS_HPP
#define COBALT_AST_FUNCS_HPP
#include "cobalt/ast/ast.hpp"
#include "cobalt/types/types.hpp"
namespace cobalt::ast {
  struct binop_ast : ast_base {
    sstring op;
    AST lhs, rhs;
    binop_ast(location loc, sstring op, AST&& lhs, AST&& rhs) : ast_base(loc), op(op), CO_INIT(lhs), CO_INIT(rhs) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<binop_ast const*>(other) return op == ptr->op && lhs == ptr->lhs && rhs == ptr->rhs; else return false;}
  };
  struct call_ast {
    sstring name;
    std::vector<AST> args;
    binop_ast(location loc, sstring name, std::vector<AST>&& args) : ast_base(loc), name(name), CO_INIT(args) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<call_ast const*>(other) return op == ptr->op && args == ptr->args; else return false;}
  };
  struct fndef_ast {
    sstring name;
    std::vector<type_ptr> args;
    fndef_ast(location loc, sstring name, std::vector<type_ptr>&& args) : ast_base(loc), name(name), CO_INIT(args) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<call_ast const*>(other) return op == ptr->op && args == ptr->args; else return false;}
  };
}
#endif