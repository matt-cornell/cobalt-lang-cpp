#ifdef __cplusplus
#define DEMANGLE extern "C"
#else
#define DEMANGLE
#endif
#define EXPORT DEMANGLE
#ifdef __cplusplus
#define IMPORT DEMANGLE 
#else
#define IMPORT extern
#endif
#define CALL