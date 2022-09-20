#include "cobalt/tokenizer.hpp"
#include <iostream>
#include <fstream>
void pretty_print(cobalt::token const& tok) {
  constexpr char chars[] = "0123456789abcdef";
  std::cout << tok.loc << ":\t";
  if (tok.data.size()) {
    char c = tok.data.front();
    if (c >= '0' && c <= '9') {
      std::cout.put(c);
      auto it = tok.data.begin();
      while (++it != tok.data.end()) {
        std::cout.put('\\').put('x').put(chars[(unsigned char)(*it) >> 4]).put(chars[*it & 15]);
      }
    }
    else std::cout << tok.data;
  }
  std::cout.put('\n').flush();
}
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
    for (auto const& tok : toks) pretty_print(tok);
    fail |= h.errors;
  }
  return fail;
}