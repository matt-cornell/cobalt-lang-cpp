#ifndef COBALT_FLAGS_HPP
#define COBALT_FLAGS_HPP
#include "support/error-handling.hpp"
namespace cobalt {
  struct flags_t {
    bool name_escapes = false; // "\N{LATIN CAPITAL LETTER A}" == "A"
    bool warn_whitespace = true; // warns if source includes weird whitespace characters like U+00A0
    bool update_location = true; // 
    error_handler onerror = default_handler;
  };
  inline flags_t default_flags;
}
#endif