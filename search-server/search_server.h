#pragma once

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include "concurrent_map.h"

#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <map>
#include <set>
#include <tuple>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <execution>
#include <functional>
#include <deque>

const int MAX_RESULT_DOCUMENT_COUNT = 5;

const int BUCKETS_COUNT = 100;

class SearchServer
{
public:
    template <typename StringContainer>
    explicit SearchServer(StringContainer stop_words);

    explicit SearchServer(std::string_view stop_words_text);

    explicit SearchServer(const std::string &stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status,
                     const std::vector<int> &ratings);

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy &, int document_id);

    void RemoveDocument(const std::execution::parallel_policy &, int document_id);

    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy &policy, std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy &policy, std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy &policy, std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    int GetDocumentCount() const;

    typename std::set<int>::const_iterator begin() const;

    typename std::set<int>::const_iterator end() const;

    std::map<std::string_view, double> GetWordFrequencies(int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query,
                                                                            int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy &, std::string_view raw_query,
                                                                            int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy &, std::string_view raw_query,
                                                                            int document_id) const;

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    const std::set<std::string, std::less<>> stop_words_;

    std::set<std::string> words_;

    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;

    std::map<int, DocumentData> documents_;

    std::set<int> document_ids_;

    std::map<int, std::map<std::string_view, double>> id_to_word_freqs_;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsViewNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int> &ratings);

    struct QueryWord
    {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query
    {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy &, Query &query,
                                           DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(Query &query,
                                           DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy &, Query &query,
                                           DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(StringContainer stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord))
    {
        throw std::invalid_argument("Some of stop words are invalid");
    }
}

template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy &policy, std::string_view raw_query,
                                                     DocumentPredicate document_predicate) const
{
    auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    std::sort(policy, matched_documents.begin(), matched_documents.end(),
              [](const Document &lhs, const Document &rhs)
              {
                  return lhs.relevance > rhs.relevance || (std::abs(lhs.relevance - rhs.relevance) < 1e-6 && lhs.rating > rhs.rating);
              });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
    {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy &policy, std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(
        policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating)
        { return document_status == status; });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy &policy, std::string_view raw_query) const
{
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
                                                     DocumentPredicate document_predicate) const
{
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy &, Query &query,
                                                     DocumentPredicate document_predicate) const
{
    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
        {
            const auto &document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating))
            {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (std::string_view word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        for (auto [document_id, _] : word_to_document_freqs_.at(word))
        {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance)
    {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(Query &query,
                                                     DocumentPredicate document_predicate) const
{
    return SearchServer::FindAllDocuments(std::execution::seq, query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy &, Query &query,
                                                     DocumentPredicate document_predicate) const
{
    ConcurrentMap<int, double> pre_document_to_relevance(BUCKETS_COUNT);

    std::for_each(std::execution::par,
                  query.plus_words.begin(),
                  query.plus_words.end(),
                  [this, &pre_document_to_relevance, document_predicate](const std::string_view &word)
                  {
                      if (word_to_document_freqs_.count(word) == 0)
                          return;

                      const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

                      for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
                      {
                          const auto &document_data = documents_.at(document_id);
                          if (document_predicate(document_id, document_data.status, document_data.rating))
                          {
                              pre_document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                          }
                      }
                  });

    auto document_to_relevance = pre_document_to_relevance.BuildOrdinaryMap();

    std::for_each(std::execution::par,
                  query.minus_words.begin(),
                  query.minus_words.end(),
                  [this, &document_to_relevance](const std::string_view &word)
                  {
                      if (word_to_document_freqs_.count(word) == 0)
                          return;
                      for (const auto [document_id, _] : word_to_document_freqs_.at(word))
                      {
                          document_to_relevance.erase(document_id);
                      }
                  });

    std::vector<Document> matched_documents(document_to_relevance.size());
    std::transform(std::execution::par, document_to_relevance.begin(), document_to_relevance.end(), matched_documents.begin(), [this](const auto &map)
                   { return Document{map.first, map.second, documents_.at(map.first).rating}; });
    return matched_documents;
}

void AddDocument(SearchServer &search_server, int document_id, std::string_view document,
                 DocumentStatus status, const std::vector<int> &ratings);

void FindTopDocuments(const SearchServer &search_server, std::string_view raw_query);

void MatchDocuments(const SearchServer &search_server, std::string_view query);
