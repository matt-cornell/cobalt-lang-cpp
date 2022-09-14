#include "std.h"
#include <cstdio>
EXPORT int __co_IOFBF = _IOFBF, __co_IOLBF = _IOLBF, __co_IONBF = _IONBF;
EXPORT int __co_SEEK_SET = SEEK_SET, __co_SEEK_CUR = SEEK_CUR, __co_SEEK_END = SEEK_END;
EXPORT void* CALL __co_stdin() {return stdin;}
EXPORT void* CALL __co_stdout() {return stdout;}
EXPORT void* CALL __co_stderr() {return stderr;}
EXPORT void* CALL __co_fopen(int8_t const* name, int8_t const* mode) {return std::fopen((char*)name, (char*)mode);}
EXPORT void* CALL __co_freopen(int8_t const* name, int8_t const* mode, void* stream) {return std::freopen((char*)name, (char*)mode, (std::FILE*)stream);}
EXPORT int32_t CALL __co_fclose(void* stream) {return std::fclose((std::FILE*)stream);}
EXPORT int32_t CALL __co_fflush(void* stream) {return std::fflush((std::FILE*)stream);}
EXPORT void CALL __co_setbuf(void* stream, int8_t* buffer) {std::setbuf((std::FILE*)stream, (char*)buffer);}
EXPORT int32_t CALL __co_setvbuf(void* stream, int8_t* buffer, int mode, uint64_t size) {return std::setvbuf((std::FILE*)stream, (char*)buffer, mode, size);}
EXPORT uint64_t CALL __co_fread(void* buffer, uint64_t size, void* stream) {return std::fread(buffer, size, 1, (std::FILE*)stream);}
EXPORT uint64_t CALL __co_fwrite(void* buffer, uint64_t size, void* stream) {return std::fwrite(buffer, size, 1, (std::FILE*)stream);}
EXPORT optional_int8_t CALL __co_fgetc(void* stream) {
  auto res = std::fgetc((std::FILE*)stream);
  if (res == EOF) return {false, 0};
  else return {true, (int8_t)res};
}
EXPORT optional_int8_t CALL __co_fputc(uint8_t ch, void* stream) {
  auto res = std::fputc(ch, (std::FILE*)stream);
  if (res == EOF) return {false, 0};
  else return {true, (int8_t)res};
}
EXPORT int64_t CALL __co_ftell(void* stream) {return std::ftell((std::FILE*)stream);}
EXPORT bool CALL __co_fseek(void* stream, int64_t offset, int8_t origin) {return std::fseek((std::FILE*)stream, offset, origin) == 0;}