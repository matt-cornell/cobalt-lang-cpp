#ifndef COBALT_SUPPORT_LOCATION_HPP
#define COBALT_SUPPORT_LOCATION_HPP
#include "sstring.hpp"
#include <llvm/ADT/Twine.h>
#include <llvm/Support/raw_ostream.h>
namespace cobalt {
  struct location {
    sstring::string file;
    std::size_t line, col;
    std::string format() const {return (llvm::Twine(llvm::StringRef(file)) + ":" + llvm::Twine(line) + ":" + llvm::Twine(col)).str();}
  };
  inline bool operator==(location const& lhs, location const& rhs) {return lhs.file == rhs.file && lhs.line == rhs.line && lhs.col == rhs.col;}
  inline bool operator!=(location const& lhs, location const& rhs) {return lhs.file != rhs.file || lhs.line != rhs.line || lhs.col != rhs.col;}
  inline llvm::raw_ostream& operator<<(llvm::raw_ostream& os, location const& loc) {return os << loc.file << ':' << loc.line << ':' << loc.col;}
}
#endif