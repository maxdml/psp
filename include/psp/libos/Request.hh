#ifndef PSP_REQUEST_H
#define PSP_REQUEST_H

#include <psp/time.hh>
#include <psp/annot.h>
#include <psp/logging.hh>
#include <base/compiler.h>
#include <string.h>

enum class ReqType {
    UNKNOWN = 0,
    //Microbenchmarks requests
    SHORT,
    LONG,
    //REST API requests
    PAGE,
    REGEX,
    // Silo requests
    NEW_ORDER,
    PAYMENT,
    DELIVERY,
    ORDER_STATUS,
    STOCK_LEVEL,
    LAST
};

[[gnu::unused]] static const char *req_type_str[] = {
    "UNKNOWN",
    "SHORT",
    "LONG",
    "PAGE",
    "REGEX",
    "NewOrder",
    "Payment",
    "Delivery",
    "OrderStatus",
    "StockLevel"
};

[[gnu::unused]] static enum ReqType str_to_type(std::string const &type) {
   if (type == "REGEX") {
        return ReqType::REGEX;
    } else if (type == "PAGE") {
        return ReqType::PAGE;
    } else if (type == "Payment") {
        return ReqType::PAYMENT;
    } else if (type == "NewOrder") {
        return ReqType::NEW_ORDER;
    } else if (type == "Delivery") {
        return ReqType::DELIVERY;
    } else if (type == "StockLevel") {
        return ReqType::STOCK_LEVEL;
    } else if (type == "OrderStatus") {
        return ReqType::ORDER_STATUS;
    }
    return ReqType::UNKNOWN;
}


// IX messages format
struct IXMessage {
    uint16_t type;
    uint16_t seq_num;
    uint32_t queue_length[3];
	uint16_t client_id;
	uint32_t req_id;
	uint32_t pkts_length;
	uint64_t runNs;
	uint64_t genNs;
} __attribute__((__packed__));

static inline int fast_atoi(const char * str, const char *end) {
    int val = 0;
    while (*str != ' ' && str < end) {
        val = val*10 + (*str++ - '0');
    }
    return val;
}

#define RQUEUE_LEN 4096
#define MAX_WORKERS 32
struct RequestType {
    enum ReqType type;
    uint64_t mean_ns;
    uint64_t deadline;
    double ratio;
    uint8_t free_peers[MAX_WORKERS];
    unsigned long rqueue[RQUEUE_LEN];
    unsigned int rqueue_tail;
    unsigned int rqueue_head;

    //Dyn resa variables
    uint32_t n_resas = 0;

    RequestType() {};

    RequestType(enum ReqType type, uint64_t mean_ns, uint64_t deadline, double ratio) :
        type(type), mean_ns(mean_ns), deadline(deadline), ratio(ratio) {
            //There must be a way to 0-init an C array from initializer list
            memset(free_peers, 0, MAX_WORKERS);
            memset(rqueue, 0, RQUEUE_LEN * sizeof(unsigned long));
            rqueue_tail = 0;
            rqueue_head = 0;
        };

    bool operator ==(const RequestType &other) const {
        return other.type == type;
    }

    bool operator !=(const RequestType &other) const {
        return other.type != type;
    }
};

#endif // PSP_REQUEST_H
