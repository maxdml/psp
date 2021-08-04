#ifndef PSP_FAIL_H_IS_INCLUDED
#define PSP_FAIL_H_IS_INCLUDED

#include "meta.h"
#include "sys/gcc.h"
#include <errno.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*psp_onfail_t)(int error_arg,
      const char *expr_arg, const char *funcn_arg, const char *filen_arg,
      int lineno_arg);

#define PSP_TRUE2(Error, Condition, ErrorCache) \
    do { \
        const int ErrorCache = (Error); \
        if (PSP_UNLIKELY(!(Condition))) { \
            psp_fail(ErrorCache, #Condition, NULL, __FILE__, __LINE__);  \
            return ErrorCache; \
        } \
   } while (0)

#define PSP_TRUE(Error, Condition) \
    PSP_TRUE2(Error, Condition, PSP_TRUE_errorCache)

#define PSP_OK2(Error, ErrorCache) \
    do { \
        const int ErrorCache = (Error); \
        if (PSP_UNLIKELY(0 != ErrorCache)) { \
            psp_fail(ErrorCache, #Error, NULL, __FILE__, __LINE__); \
            return ErrorCache; \
        } \
    } while (0)

#define PSP_OK(Error) \
    PSP_OK2((Error), PSP_UNIQID(PSP_OK_errorCache))

#define PSP_PANIC(Why) psp_panic((Why), __FILE__, __LINE__)

void psp_panic(const char *why_arg, const char *filen_arg, int lineno_arg);
void psp_onfail(psp_onfail_t onfail_arg);
void psp_fail(int error_arg, const char *expr_arg,
      const char *funcn_arg, const char *filen_arg, int lineno_arg);

#ifdef __cplusplus
}
#endif

#endif /* PSP_FAIL_H_IS_INCLUDED */
