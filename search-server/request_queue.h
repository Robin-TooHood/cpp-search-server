#pragma once

#include "search_server.h"

#include <string>
#include <vector>
#include <deque>
#include <cstdint>

class RequestQueue
{
public:
    explicit RequestQueue(const SearchServer &search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string &raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult
    {
        std::uint64_t timestamp;
        int results;
    };
    std::deque<QueryResult> requests_;
    const SearchServer &search_server_;
    int no_results_requests_;
    std::uint64_t current_time_;
    const static int min_in_day_ = 1440;

    void AddRequest(int results_num)
    {
        ++current_time_;
        while (!requests_.empty() && min_in_day_ <= current_time_ - requests_.front().timestamp)
        {
            if (0 == requests_.front().results)
            {
                --no_results_requests_;
            }
            requests_.pop_front();
        }
        requests_.push_back({current_time_, results_num});
        if (0 == results_num)
        {
            ++no_results_requests_;
        }
    }
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query, DocumentPredicate document_predicate)
{
    const auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
    AddRequest(result.size());
    return result;
}