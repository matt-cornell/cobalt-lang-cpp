#include "cobalt/ast.hpp"
using namespace cobalt;
// ast.hpp
void cobalt::ast::ast_base::print_self(llvm::raw_ostream& os, llvm::Twine name) const {os << name + "\n";}
void cobalt::ast::ast_base::print_node(llvm::raw_ostream& os, llvm::Twine prefix, AST const& ast, bool last) const {
  os << prefix + (last ? "└── ": "├── ");
  ast.print_impl(os, prefix + (last ? "    " : "│   "));
}
// flow.hpp
void cobalt::ast::top_level_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, "top level");
  if (insts.empty()) return;
  auto last = &insts.back();
  for (auto const& ast : insts) print_node(os, prefix, ast, &ast == last);
}
void cobalt::ast::group_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, "group");
  if (insts.empty()) return;
  auto last = &insts.back();
  for (auto const& ast : insts) print_node(os, prefix, ast, &ast == last);
}
void cobalt::ast::block_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, "block");
  if (insts.empty()) return;
  auto last = &insts.back();
  for (auto const& ast : insts) print_node(os, prefix, ast, &ast == last);
}
void cobalt::ast::if_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  if (if_false) {
    print_self(os, "if/else");
    print_node(os, prefix, cond, false);
    print_node(os, prefix, if_true, false);
    print_node(os, prefix, if_false, true);
  }
  else {
    print_self(os, "if");
    print_node(os, prefix, cond, false);
    print_node(os, prefix, if_true, true);
  }
}
void cobalt::ast::while_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, "while");
  print_node(os, prefix, cond, false);
  print_node(os, prefix, body, true);
}
void cobalt::ast::for_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, llvm::Twine("for: ") + elem_name);
  print_node(os, prefix, cond, false);
  print_node(os, prefix, body, true);
}
// funcs.hpp
void cobalt::ast::cast_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, llvm::Twine("cast: ") + target->name());
  print_node(os, prefix, val, true);
}
void cobalt::ast::binop_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, llvm::Twine("op: ") + op);
  print_node(os, prefix, lhs, false);
  print_node(os, prefix, rhs, true);
}
void cobalt::ast::unop_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, llvm::Twine("op: ") + op);
  print_node(os, prefix, val, true);
}
void cobalt::ast::call_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, llvm::Twine("call: ") + name);
  if (args.empty()) return;
  auto last = &args.back();
  for (auto const& ast : args) print_node(os, prefix, ast, &ast == last);
}
void cobalt::ast::fndef_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  os << llvm::Twine("fndef: ") + name + ", args: ";
  auto last = &args.back();
  for (auto const& type : args) os << (&type == last ? llvm::Twine(type->name()) + "\n" : llvm::Twine(type->name()) + ", ");
}
// literals.hpp
void cobalt::ast::integer_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  os << "int: " << val;
  if (!suffix.empty()) os << ", suffix: " << suffix;
  os << '\n';
}
void cobalt::ast::float_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  os << "float: " << val;
  if (!suffix.empty()) os << ", suffix: " << suffix;
  os << '\n';
}
void cobalt::ast::string_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  os << "int: " << val;
  if (!suffix.empty()) os << ", suffix: " << suffix;
  os << '\n';
}
void cobalt::ast::char_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  os << "int: " << val;
  if (!suffix.empty()) os << ", suffix: " << suffix;
  os << '\n';
}
// meta.hpp
void cobalt::ast::llvm_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  os << "LLVM";
  std::size_t start = 0, idx = code.find('\n');
  for (; idx != std::string::npos; start = idx, idx = code.find('\n', idx)) os << prefix << std::string_view{code.data() + start, idx - start};
  os << std::string_view{code.data() + start, code.size() - start} << '\n';
}
void cobalt::ast::asm_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  os << "asm";
  std::size_t start = 0, idx = code.find('\n');
  for (; idx != std::string::npos; start = idx, idx = code.find('\n', idx)) os << prefix << std::string_view{code.data() + start, idx - start};
  os << std::string_view{code.data() + start, code.size() - start} << '\n';
}
// scope.hpp
void cobalt::ast::module_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, llvm::Twine("module ") + name);
  if (insts.empty()) return;
  auto last = &insts.back();
  for (auto const& ast : insts) print_node(os, prefix, ast, &ast == last);
}
void cobalt::ast::import_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, llvm::Twine("import: ") + path);
}
// vars.hpp
void cobalt::ast::vardef_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, llvm::Twine("vardef: ") + name);
  print_node(os, prefix, val, true);
}
void cobalt::ast::mutdef_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, llvm::Twine("mutdef: ") + name);
  print_node(os, prefix, val, true);
}
void cobalt::ast::varget_ast::print_impl(llvm::raw_ostream& os, llvm::Twine prefix) const {
  print_self(os, llvm::Twine("varget: ") + name);
}
