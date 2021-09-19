#pragma once
#ifndef MSH_H_
#define MSH_H_

#define NET_HDR_SIZE 42 // ETH(14) + IP(20) + UDP(8)
class ClientRequest {
    public: uint32_t id;
    public: void *mbuf;
    public: uint32_t run_ns;
    public: enum ReqType type;
    public: uint64_t sending = 0;
    public: uint64_t completed = 0;
    public: uint32_t schedule_id;

    public: ClientRequest() {};

    public: static enum ReqType str_to_type(std::string const &type) {
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

    public: static const std::string LOG_COLUMNS;
    public: friend std::ostream& operator<< (std::ostream &out, const ClientRequest &req) {
        out << req.id << "\t"
        << req_type_str[static_cast<int>(req.type)] << "\t"
        << std::fixed << req.sending / cycles_per_ns << "\t"
        << std::fixed << req.completed / cycles_per_ns << "\t"
        << std::fixed << (req.completed - req.sending) / cycles_per_ns << "\t"
        << std::fixed << req.run_ns << "\t"
        << req.schedule_id;
        return out;
    }
    public: bool operator ==(const ClientRequest &b) const { return this->id == b.id and this->type == b.type; }
};

const std::string ClientRequest::LOG_COLUMNS =
    "REQ_ID\tREQ_TYPE\tSENDING\tCOMPLETED\tRESP_TIME\tMEAN_NS\tSCHED_ID";

#endif // MSH_H_
