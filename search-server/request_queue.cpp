#include "request_queue.h"

#include "document.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer &search_server)
    : search_server_(search_server), no_results_requests_(0), current_time_(0)
{
}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query, DocumentStatus status)
{
    const auto result = search_server_.FindTopDocuments(raw_query, status);
    AddRequest(result.size());
    return result;
}

vector<Document> RequestQueue::AddFindRequest(const string &raw_query)
{
    const auto result = search_server_.FindTopDocuments(raw_query);
    AddRequest(result.size());
    return result;
}

int RequestQueue::GetNoResultRequests() const
{
    return no_results_requests_;
}