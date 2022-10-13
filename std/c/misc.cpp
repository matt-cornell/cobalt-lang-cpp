#include "std.h"
#include <cstdlib>
EXPORT void CALL __co_abort(int code) {std::abort(code);}
EXPORT void CALL __co_exit(int code) {std::exit(code);}
EXPORT void CALL __co_quick_exit(int code) {std::quick_exit(code);}
EXPORT void CALL __co_atexit(void(*handler)()) {std::atexit(handler);}
EXPORT void CALL __co_at_quick_exit(void(*handler)()) {std::at_quick_exit(handler);}
