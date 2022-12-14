#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server) : server_(search_server) {
}
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    auto lambda = [status](int document_id, DocumentStatus status_lambda, int rating) { return status_lambda == status ;};
    return  AddFindRequest(raw_query, lambda);
}
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}
int RequestQueue::GetNoResultRequests() const {
    return empty_req_count_;
}