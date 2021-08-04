#ifndef PSP_META_H_IS_INCLUDED
#define PSP_META_H_IS_INCLUDED

#define PSP_META_CONCAT2(A, B) A##B
#define PSP_CONCAT(A, B) PSP_META_CONCAT2(A, B)
#define PSP_COUNTER __COUNTER__
#define PSP_UNIQID(Prefix) PSP_CONCAT(Prefix, PSP_COUNTER)
#define PSP_NOP() do { } while (0)

#endif /* PSP_META_H_IS_INCLUDED */
