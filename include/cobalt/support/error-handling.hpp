#ifndef COBALT_SUPPORT_ERRORS_HPP
#define COBALT_SUPPORT_ERRORS_HPP
#include "functions.hpp"
#include "location.hpp"
#include <llvm/Support/raw_ostream.h>
namespace cobalt {
  enum severity {WARNING, ERROR, CRITICAL};
  inline struct default_handler_t {
    std::size_t warnings = 0, errors = 0;
    bool critical = false;
    void operator()(location loc, std::string_view err, severity sev) {
      auto& os = llvm::errs();
      switch (sev) {
        case WARNING:
          ++warnings;
          os << loc << ": ";
          if (os.is_displayed()) {
            os.changeColor(llvm::raw_ostream::YELLOW, true);
            os << "warning: ";
            os.resetColor();
          }
          else os << "warning: ";
          os << err << "\n";
          break;
        case ERROR:
          ++errors;
          os << loc << ": ";
          if (os.is_displayed()) {
            os.changeColor(llvm::raw_ostream::RED, true);
            os << "error: ";
            os.resetColor();
          }
          else os << "error: ";
          os << err << "\n";
          break;
        case CRITICAL:
          ++errors;
          critical = true;
          os << loc << ": ";
          if (os.is_displayed()) {
            os.changeColor(llvm::raw_ostream::MAGENTA, true);
            os << "critical: ";
            os.resetColor();
          }
          else os << "critical: ";
          os << err << "\n";
          break;
      }
    }
  } default_handler;
  inline struct werror_handler_t {
    std::size_t errors = 0;
    bool critical = false;
    void operator()(location loc, std::string_view err, severity sev) {
      auto& os = llvm::errs();
      switch (sev) {
        case WARNING:
        case ERROR:
          ++errors;
          os << loc << ": ";
          if (os.is_displayed()) {
            os.changeColor(llvm::raw_ostream::RED, true);
            os << "error: ";
            os.resetColor();
          }
          else os << "error: ";
          os << err << "\n";
          break;
        case CRITICAL:
          ++errors;
          critical = true;
          os << loc << ": ";
          if (os.is_displayed()) {
            os.changeColor(llvm::raw_ostream::MAGENTA, true);
            os << "critical: ";
            os.resetColor();
          }
          else os << "critical: ";
          os << err << "\n";
          break;
      }
    }
  } werror_handler;
  inline struct quiet_handler_t {
    std::size_t warnings = 0, errors = 0;
    bool critical = false;
    void operator()(location loc, std::string_view err, severity sev) {
      switch (sev) {
        case WARNING:
          ++warnings;
          break;
        case CRITICAL:
          critical = true;
        case ERROR:
          ++errors;
          break;
      }
    }
  } quiet_handler;
  using error_handler = borrow_function<void(location, std::string_view, severity)>;
  struct bound_handler {
    location const& loc;
    error_handler handler;
    void operator()(std::string_view err, severity sev) const {handler(loc, err, sev);}
  };
}
#endif