#include "cobalt/tokenizer.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char** argv) {
  bool fail = false;
  std::string str;
  cobalt::flags_t flags = cobalt::default_flags;
  for (auto it = argv + 1; it != argv + argc; ++it) {
    cobalt::default_handler_t h;
    flags.onerror = h;
    std::string_view file = *it;
    if (file == "-") str.assign(std::istreambuf_iterator<char>{std::cin}, {});
    else {
      std::ifstream ifs(file.data());
      str.assign(std::istreambuf_iterator<char>{ifs}, {});
    }
    auto toks = cobalt::tokenize(str, cobalt::sstring::get(file == "-" ? "<stdin>" : file));
    for (auto const& tok : toks) std::cout << tok.loc << '\t' << tok.data << std::endl;
    fail |= h.errors;
  }
  return fail;
}