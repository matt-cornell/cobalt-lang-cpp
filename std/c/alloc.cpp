#include "std.h"
#include <cstdlib>
EXPORT void* CALL __co_alloc(uint64_t size, uint8_t align) {return std::malloc(size);}
EXPORT void CALL __co_free(void* ptr) {std::free(ptr);}