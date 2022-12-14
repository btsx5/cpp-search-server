#pragma once
#include "search_server.h"
#include <stack>
class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        bool empty_req;
        std::string request;
        std::vector<Document> query_result;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& server_;
    int time = 0;
    int empty_req_count_= 0;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    ++time;
    std::vector<Document> result = server_.FindTopDocuments(raw_query, document_predicate);
    if (time<=min_in_day_) {
        if (result.empty()) {
            requests_.push_back({true, raw_query, result});
            ++empty_req_count_;
            return result;
        } else {
            requests_.push_back({false, raw_query, result});
            return result;
        }

    }
    else {
        if (result.empty()) {
            if (!requests_.front().empty_req) {
                ++empty_req_count_;
            }
            requests_.pop_front();
            requests_.push_back({true, raw_query, result});
            return result;

        } else {
            if (requests_.front().empty_req) {
                --empty_req_count_;
            }
            requests_.pop_front();
            requests_.push_back({false, raw_query, result});
            return result;
        }
    }
    // напишите реализацию
}