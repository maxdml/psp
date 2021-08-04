#ifndef PSP_ANNOT_H_IS_INCLUDED
#define PSP_ANNOT_H_IS_INCLUDED

#include "fail.h"
#include "meta.h"

#define PSP_UNUSEDARG(ArgName) (void)(ArgName)
#define PSP_ZERO(Error, Value) PSP_TRUE((Error), 0 == (Value))
#define PSP_NONZERO(Error, Value) PSP_TRUE((Error), 0 != (Value))
#define PSP_NULL(Error, Value) PSP_TRUE((Error), NULL == (Value))
#define PSP_NOTNULL(Error, Value) PSP_TRUE((Error), NULL != (Value))

#define PSP_UNREACHABLE() \
    do { \
        PSP_PANIC("unreachable code"); \
        return -1; \
    } while (0)

#define PSP_FAIL(Error) \
    do { \
        PSP_OK(Error); \
        PSP_UNREACHABLE(); \
    } while (0)



#endif /* PSP_ANNOT_H_IS_INCLUDED */
