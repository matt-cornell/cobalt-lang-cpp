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
#endif