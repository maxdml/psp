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
    GET,
    SCAN,
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
    "StockLevel",
    "GET",
    "SCAN"
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
    } else if (type == "SHORT") {
        return ReqType::SHORT;
    } else if (type == "LONG") {
        return ReqType::LONG;
    } else if (type == "GET") {
        return ReqType::GET;
    } else if (type == "SCAN") {
        return ReqType::SCAN;
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
#define MAX_TYPES 8
#define QOS_FACTOR 10
struct RequestType {
    enum ReqType type;
    uint64_t mean_ns = 0;
    uint64_t deadline = 0;
    double ratio = 0;
    unsigned long rqueue[RQUEUE_LEN];
    unsigned int rqueue_tail;
    unsigned int rqueue_head;
    uint64_t tsqueue[RQUEUE_LEN];

    // Profiling variables
    uint64_t windows_mean_ns = 0;
    uint64_t windows_count = 0;
    uint64_t delay = 0;

    //DARC variables
    uint32_t res_peers[MAX_WORKERS];
    uint32_t n_resas = 0;
    uint64_t last_resa;
    uint32_t stealable_peers[MAX_WORKERS];
    uint32_t n_stealable = 0;
    int type_group = -1;
    uint64_t max_delay = 0;
    double prev_demand = 0;

    RequestType() {};

    RequestType(enum ReqType type, uint64_t mean_ns, uint64_t deadline, double ratio) :
        type(type), mean_ns(mean_ns), deadline(deadline), ratio(ratio) {
            //There must be a way to 0-init an C array from initializer list
            memset(rqueue, 0, RQUEUE_LEN * sizeof(unsigned long));
            rqueue_tail = 0;
            rqueue_head = 0;
        };

    bool operator ==(const RequestType &other) const {
        return other.type == type;
    }

    bool operator <(const RequestType &other) const {
        /*
        if (ratio == 0)
            return false;
        */
        return mean_ns < other.mean_ns;
    }

    bool operator !=(const RequestType &other) const {
        return other.type != type;
    }
};

struct TypeGroups {
    RequestType *members[MAX_TYPES]; // By construction members are sorted by ascending service time
    uint32_t n_members = 0;
    uint32_t res_peers[MAX_WORKERS];
    uint32_t n_resas = 0;
    uint32_t stealable_peers[MAX_WORKERS];
    uint32_t n_stealable = 0;
};

#endif // PSP_REQUEST_H
