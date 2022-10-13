#ifndef COBALT_AST_LITERALS_HPP
#define COBALT_AST_LITERALS_HPP
#include "cobalt/ast/ast.hpp"
#include <llvm/ADT/APInt.h>
namespace cobalt::ast {
  struct literal_ast : ast_base {
    sstring suffix;
    literal_ast(location loc, sstring suffix) : ast_base(loc), suffix(suffix) {}
    ~literal_ast();
  };
  inline literal_ast::~literal_ast() {}
  struct integer_ast : literal_ast {
    llvm::APInt val;
    integer_ast(location loc, llvm::APInt val, sstring suffix) : literal_ast(loc, suffix), val(std::move(val)) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<integer_ast const*>(other)) return suffix == ptr->suffix && val == ptr->val; else return false;}
  private:
    typed_value codegen_impl(compile_context& ctx) const override;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct float_ast : literal_ast {
    double val;
    float_ast(location loc, double val, sstring suffix) : literal_ast(loc, suffix), val(val) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<float_ast const*>(other)) return suffix == ptr->suffix && val == ptr->val; else return false;}
  private:
    typed_value codegen_impl(compile_context& ctx) const override;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct string_ast : literal_ast {
    std::string val;
    string_ast(location loc, std::string&& val, sstring suffix) : literal_ast(loc, suffix), val(std::move(val)) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<string_ast const*>(other)) return suffix == ptr->suffix && val == ptr->val; else return false;}
  private:
    typed_value codegen_impl(compile_context& ctx) const override;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
  struct char_ast : literal_ast {
    std::string val; // string for multibyte chars
    char_ast(location loc, std::string&& val, sstring suffix) : literal_ast(loc, suffix), val(std::move(val)) {}
    bool eq(ast_base const* other) const override {if (auto ptr = dynamic_cast<char_ast const*>(other)) return suffix == ptr->suffix && val == ptr->val; else return false;}
  private:
    typed_value codegen_impl(compile_context& ctx) const override;
    void print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const override;
  };
}
#endif
