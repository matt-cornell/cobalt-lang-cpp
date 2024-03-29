#ifndef COBALT_AST_VARS_HPP
#define COBALT_AST_VARS_HPP
#include "cobalt/ast/ast.hpp"
namespace cobalt {
  namespace ast {
    struct vardef_ast : ast_base {
      sstring name;
      AST val;
      bool global;
      std::vector<std::string> annotations = {};
      vardef_ast(location loc, sstring name, AST&& val, bool global = false, std::vector<std::string>&& annotations = {}) : ast_base(loc), name(name), CO_INIT(val), global(global), CO_INIT(annotations) {}
      bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<vardef_ast const*>(other)) return name == ptr->name && val == ptr->val; else return false;}
      typed_value codegen(compile_context& ctx) const override;
      type_ptr type(base_context& ctx) const override;
    private: 
      void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
    };
    struct mutdef_ast : ast_base {
      sstring name;
      AST val;
      bool global;
      std::vector<std::string> annotations = {};
      mutdef_ast(location loc, sstring name, AST&& val, bool global = false, std::vector<std::string>&& annotations = {}) : ast_base(loc), name(name), CO_INIT(val), global(global), CO_INIT(annotations) {}
      bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<mutdef_ast const*>(other)) return name == ptr->name && val == ptr->val; else return false;}
      typed_value codegen(compile_context& ctx) const override;
      type_ptr type(base_context& ctx) const override;
    private: 
      void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
    };
    struct varget_ast : ast_base {
      sstring name;
      varget_ast(location loc, sstring name) : ast_base(loc), name(name) {}
      bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<varget_ast const*>(other)) return name == ptr->name; else return false;}
      typed_value codegen(compile_context& ctx) const override;
      type_ptr type(base_context& ctx) const override;
    private: 
      void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
    };
  }
}
#endif