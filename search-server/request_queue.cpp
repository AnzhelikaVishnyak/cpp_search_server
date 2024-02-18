#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    return RequestQueue::AddFindRequest(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {return document_status == status; });
}
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return RequestQueue::AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}


int RequestQueue::GetNoResultRequests() const {
    return num_no_result_requests_;
}