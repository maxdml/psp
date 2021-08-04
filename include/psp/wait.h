// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef PSP_WAIT_H_IS_INCLUDED
#define PSP_WAIT_H_IS_INCLUDED

#include <psp/sys/gcc.h>
#include <psp/types.h>

#ifdef __cplusplus
extern "C" {
#endif

PSP_EXPORT int psp_wait(psp_qresult_t *qr_out, psp_qtoken_t qt);
PSP_EXPORT int psp_wait_any(psp_qresult_t *qr_out, int *start_offset,
                              int *ready_offset, psp_qtoken_t qts[], int num_qts);

#ifdef __cplusplus
}
#endif

#endif /* PSP_WAIT_H_IS_INCLUDED */
