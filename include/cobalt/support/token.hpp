#ifndef COBALT_SUPPORT_TOKEN_HPP
#define COBALT_SUPPORT_TOKEN_HPP
#include "cobalt/support/location.hpp"
#include <string>
namespace cobalt {
  struct token {
    location loc;
    std::string data;
  };
}
#endif