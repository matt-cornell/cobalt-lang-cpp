#ifndef COBALT_SUPPORT_TOKEN_HPP
#define COBALT_SUPPORT_TOKEN_HPP
#include "cobalt/support/location.hpp"
#include <string>
namespace cobalt {
  struct token {
    location loc;
    std::string data;
  };
  inline bool operator==(token const& lhs, token const& rhs) {return lhs.loc == rhs.loc && lhs.data == rhs.data;}
  inline bool operator!=(token const& lhs, token const& rhs) {return lhs.loc != rhs.loc || lhs.data != rhs.data;}
}
#endif