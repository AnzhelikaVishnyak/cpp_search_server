#pragma once
#include <vector>
#include <deque>
#include <string>
#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server), num_no_result_requests_(0), current_time_(0)
    {
    }

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        std::vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);
        ++current_time_;

        while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().timestamp) {
            if (requests_.front().size_query_result == 0) {
                --num_no_result_requests_;
            }
            requests_.pop_front();
        }

        QueryResult query_result = { current_time_,  result.size()};
        requests_.emplace_back(query_result);

        if (result.empty()) {
            ++num_no_result_requests_;
        }
        return result;

    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        uint64_t timestamp;
        size_t size_query_result;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;

    const SearchServer& search_server_;
    int num_no_result_requests_;
    uint64_t current_time_;
};
