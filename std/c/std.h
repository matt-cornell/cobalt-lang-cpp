#ifndef LIBCOSTD_H
#define LIBCOSTD_H
#include "compat.h"
#include <stdint.h>
#define DEF_OPTIONAL(T) struct optional_ ## T {bool selector; T value;};
DEF_OPTIONAL(int8_t)
// allocation
IMPORT void* CALL __co_alloc(uint64_t size, uint8_t align);
IMPORT void  CALL __co_free(void* ptr);
// file I/O
IMPORT int __co_IOFBF, __co_IOLBF, __co_IONBF;
IMPORT int __co_SEEK_SET, __co_SEEK_CUR, __co_SEEK_END;
IMPORT void* CALL __co_stdin();
IMPORT void* CALL __co_stdout();
IMPORT void* CALL __co_stderr();
IMPORT void* CALL __co_fopen(int8_t const* name, int8_t const* mode);
IMPORT void* CALL __co_freopen(int8_t const* name, int8_t const* mode, void* stream);
IMPORT int32_t CALL __co_fclose(void* stream);
IMPORT int32_t CALL __co_fflush(void* stream);
IMPORT void CALL __co_setbuf(void* stream, int8_t* buffer);
IMPORT int32_t CALL __co_setvbuf(void* stream, int8_t* buffer, int mode, uint64_t size);
IMPORT uint64_t CALL __co_fread(void* buffer, uint64_t size, void* stream);
IMPORT uint64_t CALL __co_fwrite(void* buffer, uint64_t size, void* stream);
IMPORT optional_int8_t CALL __co_fgetc(void* stream);
IMPORT optional_int8_t CALL __co_fputc(uint8_t ch, void* stream);
IMPORT int64_t CALL __co_ftell(void* stream);
IMPORT bool CALL __co_fseek(void* stream, int64_t offset, int8_t origin);
// miscellaneous utilities
IMPORT void CALL __co_abort(int code);
IMPORT void CALL __co_exit(int code);
IMPORT void CALL __co_quick_exit(int code);
IMPORT void CALL __co_atexit(void(*handler)());
IMPORT void CALL __co_at_quick_exit(void(*handler)());
// string operations
IMPORT uint64_t CALL __co_strlen(char const* str);
IMPORT  int64_t CALL __co_strcmp(char const* lhs, char const* rhs);
IMPORT  int64_t CALL __co_strncmp(char const* lhs, char const* rhs, uint64_t size);
IMPORT char* CALL __co_strcpy(char* dst, char const* src);
IMPORT char* CALL __co_strncpy(char* dst, char const* src, uint64_t size);
IMPORT char* CALL __co_strcat(char* dst, char const* src);
IMPORT char* CALL __co_strncat(char* dst, char const* src, uint64_t size);
IMPORT int64_t CALL __co_memcmp(void const* lhs, void const* rhs, uint64_t size);
IMPORT void* CALL __co_memset(void* data, int c, uint64_t size);
IMPORT void* CALL __co_memcpy(void* dst, void const* src, uint64_t size);
IMPORT void* CALL __co_memmove(void* dst, void const* src, uint64_t size);
#endif
