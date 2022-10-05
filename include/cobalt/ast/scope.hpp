#ifndef COBALT_AST_SCOPE_HPP
#define COBALT_AST_SCOPE_HPP
#include "cobalt/ast/ast.hpp"
namespace cobalt::ast {
  struct module_ast : ast_base {
    std::string name;
    std::vector<AST> insts;
    module_ast(location loc, std::string&& name, std::vector<AST>&& insts) : ast_base(loc), CO_INIT(name), CO_INIT(insts) {}
    ~module_ast();
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<module_ast const*>(other)) return name == ptr->name && insts == ptr->insts; else return false;}
  private:
    typed_value codegen_impl(compile_context& ctx) const override;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct import_ast : ast_base {
    std::string path;
    import_ast(location loc, std::string&& path) : ast_base(loc), CO_INIT(path) {}
    ~import_ast();
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<import_ast const*>(other)) return path == ptr->path; else return false;}
  private:
    typed_value codegen_impl(compile_context& ctx) const override;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };


  inline module_ast::~module_ast() {}
  inline import_ast::~import_ast() {}
}
#endif
