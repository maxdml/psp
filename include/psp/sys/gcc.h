#ifndef PSP_SYS_GCC_H_IS_INCLUDED
#define PSP_SYS_GCC_H_IS_INCLUDED

#define PSP_EXPORT __attribute__((visibility("default")))
#define PSP_UNLIKELY(Cond) __builtin_expect((Cond), 0)
#define PSP_LIKELY(Cond) __builtin_expect((Cond), 1)

#endif /* PSP_SYS_GCC_H_IS_INCLUDED */
