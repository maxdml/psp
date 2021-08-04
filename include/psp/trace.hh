// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef PSP_TRACE_H_IS_INCLUDED
#define PSP_TRACE_H_IS_INCLUDED

#include <functional>
#include <vector>
#include <memory>
#include <unordered_map>

#include <psp/time.hh>
#include <psp/types.h>

typedef struct psp_qtoken_trace {
    uint32_t ctx_id;
    enum psp_opcode type;
    psp_qtoken_t token;
    bool start; /** < Is this the beginning of the operation */
    bool wait; /** < Is the operation waiting to be processed in the co-routine? */
    tp timestamp;
    int nic_rq_status;
} psp_qtoken_trace_t;

typedef struct psp_trace {
    std::vector<psp_qtoken_trace_t> traces;
} psp_trace_t;

typedef std::unique_ptr<psp_trace_t, std::function<void(psp_trace_t *)> > trace_ptr_type;

int psp_dump_trace_to_file(const char * filename, psp_trace_t *trace);
int psp_record_trace(psp_trace_t &trace, psp_qtoken_trace_t &qt_trace);
int psp_new_trace(psp_trace_t **trace_out, const char *name);
int psp_delete_trace(psp_trace_t **trace);
int psp_register_trace(std::string &trace_path, trace_ptr_type &traces);

#endif /* PSP_TRACE_H_IS_INCLUDED */
