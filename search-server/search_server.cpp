#include "search_server.h"
#include "document.h"

using namespace std;

SearchServer::SearchServer(const string &stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))

{
}

void SearchServer::AddDocument(int document_id, const string &document, DocumentStatus status,
                               const vector<int> &ratings)
{
    if ((document_id < 0) || (documents_.count(document_id) > 0))
    {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string &word : words)
    {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        id_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.push_back(document_id);
}

void SearchServer::RemoveDocument(int document_id)
{
    if ((document_id < 0) || (documents_.count(document_id) < 1))
    {
        throw invalid_argument("Invalid document_id"s);
    }
    for (auto it = document_ids_.begin(); it != document_ids_.end();)
    {
        if (*it == document_id)
            it = document_ids_.erase(it);
        else
            ++it;
    }
    documents_.erase(document_id);
    // word_to_document_freqs_
    for (auto [word, freq] : GetWordFrequencies(document_id))
    {
        for (auto it = word_to_document_freqs_.at(word).begin(); it != word_to_document_freqs_.at(word).end();)
        {
            if (it->first == document_id)
            {
                word_to_document_freqs_.at(word).erase(it++);
                break;
            }
            else
            {
                ++it;
            }
        }
    }
    for (auto it = id_to_word_freqs_.begin(); it != id_to_word_freqs_.end();)
    {
        if (it->first == document_id)
            it = id_to_word_freqs_.erase(it);
        else
            ++it;
    }
}

vector<Document> SearchServer::FindTopDocuments(const string &raw_query, DocumentStatus status) const
{
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating)
        { return document_status == status; });
}

vector<Document> SearchServer::FindTopDocuments(const string &raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const
{
    return documents_.size();
}
/*
int SearchServer::GetDocumentId(int index) const
{
    return document_ids_.at(index);
}
*/
typename vector<int>::const_iterator SearchServer::begin() const
{
    return document_ids_.begin();
}

typename vector<int>::const_iterator SearchServer::end() const
{
    return document_ids_.end();
}

auto SearchServer::at(int index) const
{
    return document_ids_.at(index);
}

const map<string, double> &SearchServer::GetWordFrequencies(int document_id) const
{
    if (id_to_word_freqs_.empty())
    {
        static const map<string, double> empty_map;
        return empty_map;
    }
    return id_to_word_freqs_.at(document_id);
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string &raw_query,
                                                                  int document_id) const
{
    const auto query = ParseQuery(raw_query);

    vector<string> matched_words;
    for (const string &word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            matched_words.push_back(word);
        }
    }
    for (const string &word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

void AddDocument(SearchServer &search_server, int document_id, const string &document,
                 DocumentStatus status, const vector<int> &ratings)
{
    try
    {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const exception &e)
    {
        cout << "Error in adding document "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer &search_server, const string &raw_query)
{
    cout << "Results for request: "s << raw_query << endl;
    try
    {
        for (const Document &document : search_server.FindTopDocuments(raw_query))
        {
            PrintDocument(document);
        }
    }
    catch (const exception &e)
    {
        cout << "Error is seaching: "s << e.what() << endl;
    }
    LOG_DURATION_STREAM("Find top documents"s, cerr);
}

void MatchDocuments(const SearchServer &search_server, const string &query)
{
    try
    {
        cout << "Matching for request: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index)
        {
            // const int document_id = search_server.GetDocumentId(index);
            const int document_id = search_server.at(index);

            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception &e)
    {
        cout << "Error in matchig request "s << query << ": "s << e.what() << endl;
    }
    LogDuration guard("Match document", cerr);
}