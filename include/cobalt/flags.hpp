#ifndef COBALT_FLAGS_HPP
#define COBALT_FLAGS_HPP
#include "support/error-handling.hpp"
namespace cobalt {
  struct flags_t {
    bool warn_whitespace = true; // warns if source includes weird whitespace characters like U+00A0
    error_handler onerror = default_handler;
    // internal use
    bool update_location = true;
    std::string_view exit_chars = ";";
  };
  inline flags_t default_flags;
}
#endif
