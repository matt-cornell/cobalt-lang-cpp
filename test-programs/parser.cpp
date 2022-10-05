#include "cobalt/tokenizer.hpp"
#include "cobalt/parser.hpp"
#include "cobalt/ast.hpp"
#include <iostream>
#include <fstream>
int main(int argc, char** argv) {
  cobalt::flags_t flags = cobalt::default_flags;
  cobalt::default_handler_t handler;
  flags.onerror = handler;
  bool fail = false;
  std::string str;
  for (auto it = argv + 1; it != argv + argc; ++it) {
    handler = cobalt::default_handler;
    std::string_view file = *it;
    if (file == "-") str.assign(std::istreambuf_iterator<char>{std::cin}, {});
    else {
      std::ifstream ifs(file.data());
      str.assign(std::istreambuf_iterator<char>{ifs}, {});
    }
    auto toks = cobalt::tokenize(str, cobalt::sstring::get(file == "-" ? "<stdin>" : file));
    cobalt::AST ast = cobalt::parse({toks.begin(), toks.end()}, flags);
    ast.print();
    fail |= handler.errors;
  }
  return fail;
}