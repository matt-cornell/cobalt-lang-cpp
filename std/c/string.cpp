#include "std.h"
#include <cstring>
IMPORT uint64_t CALL __co_strlen(char const* str) {return std::strlen(str);}
IMPORT  int64_t CALL __co_strcmp(char const* lhs, char const* rhs) {return std::strcmp(lhs, rhs);}
IMPORT  int64_t CALL __co_strncmp(char const* lhs, char const* rhs, uint64_t size) {return std::strncmp(lhs, rhs, size);}
IMPORT char* CALL __co_strcpy(char* dst, char const* src) {return std::strcpy(dst, src);}
IMPORT char* CALL __co_strncpy(char* dst, char const* src, uint64_t size) {return std::strncpy(dst, src, size);}
IMPORT char* CALL __co_strcat(char* dst, char const* src) {return std::strcat(dst, src);}
IMPORT char* CALL __co_strncat(char* dst, char const* src, uint64_t size) {return std::strncat(dst, src, size);}
IMPORT int64_t CALL __co_memcmp(void const* lhs, void const* rhs, uint64_t size) {return std::memcmp(lhs, rhs);}
IMPORT void* CALL __co_memset(void* data, int c, uint64_t size) {return std::memset(data, c, size);}
IMPORT void* CALL __co_memcpy(void* dst, void const* src, uint64_t size) {return std::memcpy(dst, src, size);}
IMPORT void* CALL __co_memmove(void* dst, void const* src, uint64_t size) {return std::memmove(dst, src, size);}
