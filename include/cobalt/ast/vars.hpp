#ifndef COBALT_AST_VARS_HPP
#define COBALT_AST_VARS_HPP
#include "cobalt/ast/ast.hpp"
namespace cobalt {
  namespace ast {
    struct vardef_ast : ast_base {
      sstring name;
      AST val;
      vardef_ast(location loc, sstring name, AST&& val) : ast_base(loc), name(name), CO_INIT(val) {}
      bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<vardef_ast const*>(other)) return name == ptr->name && val == ptr->val; else return false;}
    private: typed_value codegen_impl(compile_context& ctx) const override;
    };
    struct mutdef_ast : ast_base {
      sstring name;
      AST val;
      mutdef_ast(location loc, sstring name, AST&& val) : ast_base(loc), name(name), CO_INIT(val) {}
      bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<mutdef_ast const*>(other)) return name == ptr->name && val == ptr->val; else return false;}
    private: typed_value codegen_impl(compile_context& ctx) const override;
    };
    struct varget_ast : ast_base {
      sstring name;
      varget_ast(location loc, sstring name) : ast_base(loc), name(name) {}
      bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<varget_ast const*>(other)) return name == ptr->name; else return false;}
      private: typed_value codegen_impl(compile_context& ctx) const override;
    };
  }
}
#endif