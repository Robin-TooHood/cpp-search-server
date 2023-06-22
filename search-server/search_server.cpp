#include "search_server.h"

using namespace std;

SearchServer::SearchServer(string_view stop_words_text)
    : SearchServer(SplitIntoWordsView(stop_words_text))

{
}

SearchServer::SearchServer(const string &stop_words_text)
    : SearchServer(string_view(stop_words_text))

{
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status,
                               const vector<int> &ratings)
{
    if ((document_id < 0) || (documents_.count(document_id) > 0))
    {
        throw invalid_argument("Invalid document_id"s);
    }
    auto words = SplitIntoWordsViewNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    string word_original;
    string_view word_view;
    for (auto word : words)
    {
        word_original = word;
        words_.insert(word_original);
        word_view = *words_.find(word_original);
        word_to_document_freqs_[word_view][document_id] += inv_word_count;
        id_to_word_freqs_[document_id][word_view] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

void SearchServer::RemoveDocument(int document_id)
{
    RemoveDocument(execution::seq, document_id);
}

void SearchServer::RemoveDocument(const execution::sequenced_policy &, int document_id)
{
    if (!document_ids_.count(document_id))
    {
        return;
    }
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    for (auto [word, freq] : GetWordFrequencies(document_id))
    {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    id_to_word_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(const execution::parallel_policy &, int document_id)
{
    if (!document_ids_.count(document_id))
    {
        return;
    }
    document_ids_.erase(document_id);
    documents_.erase(document_id);
    auto word_freq = id_to_word_freqs_.at(document_id);
    vector<string_view> ptrs_to_words(word_freq.size());
    transform(execution::par, word_freq.begin(), word_freq.end(), ptrs_to_words.begin(), [](auto word)
              { return word.first; });
    for_each(execution::par, ptrs_to_words.begin(), ptrs_to_words.end(),
             [this, document_id](auto word)
             {
                 word_to_document_freqs_.at(word).erase(document_id);
             });
    id_to_word_freqs_.erase(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(
        execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating)
        { return document_status == status; });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const
{
    return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const
{
    return documents_.size();
}

typename set<int>::const_iterator SearchServer::begin() const
{
    return document_ids_.begin();
}

typename set<int>::const_iterator SearchServer::end() const
{
    return document_ids_.end();
}

map<string_view, double> SearchServer::GetWordFrequencies(int document_id) const
{
    if (id_to_word_freqs_.empty())
    {
        static map<string_view, double> empty_map;
        return empty_map;
    }
    return id_to_word_freqs_.at(document_id);
}

using MatchTuple = tuple<vector<string_view>, DocumentStatus>;

MatchTuple SearchServer::MatchDocument(string_view raw_query,
                                       int document_id) const
{
    return MatchDocument(execution::seq, raw_query, document_id);
}

MatchTuple SearchServer::MatchDocument(const execution::sequenced_policy &, string_view raw_query,
                                       int document_id) const
{
    if ((document_id < 0) || (documents_.count(document_id) <= 0))
    {
        throw out_of_range("Invalid document_id"s);
    }
    if (!IsValidWord(raw_query))
    {
        throw invalid_argument("Invalid query");
    }

    vector<string_view> matched_words;

    const auto query = ParseQuery(raw_query, true);

    for (string_view word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            return {matched_words, documents_.at(document_id).status};
        }
    }

    for (string_view word : query.plus_words)
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
    return {matched_words, documents_.at(document_id).status};
}

MatchTuple SearchServer::MatchDocument(const execution::parallel_policy &, string_view raw_query,
                                       int document_id) const
{
    if ((document_id < 0) || (documents_.count(document_id) <= 0))
    {
        throw out_of_range("Invalid document_id"s);
    }
    if (!IsValidWord(raw_query))
    {
        throw invalid_argument("Invalid query");
    }

    const auto query = ParseQuery(raw_query, false);

    if (any_of(execution::par, query.minus_words.begin(), query.minus_words.end(), [this, &document_id](string_view word)
               { return (word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id)); }))
    {
        return {vector<string_view>{}, documents_.at(document_id).status};
    }

    vector<string_view> matched_words(query.plus_words.size());

    auto words_end = copy_if(execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [this, &document_id](string_view word)
                             { return (word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id)); });

    sort(matched_words.begin(), words_end);
    auto last_unique = unique(matched_words.begin(), words_end);
    matched_words.erase(last_unique, matched_words.end());

    return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(string_view word) const
{
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word)
{
    return none_of(word.begin(), word.end(), [](char c)
                   { return c >= '\0' && c < ' '; });
}

vector<string_view> SearchServer::SplitIntoWordsViewNoStop(string_view text) const
{
    vector<string_view> words;
    for (string_view word : SplitIntoWordsView(text))
    {
        if (!IsValidWord(word))
        {
            string not_valid_word(word);
            throw invalid_argument("Word " + not_valid_word + " is invalid");
        }
        if (!IsStopWord(word))
        {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int> &ratings)
{
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const
{
    if (text.empty())
    {
        throw invalid_argument("Query word is empty");
    }
    string_view word = text;
    bool is_minus = false;
    if (word[0] == '-')
    {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word))
    {
        string not_valid_text(text);
        throw invalid_argument("Query word " + not_valid_text + " is invalid");
    }
    return {word, is_minus, IsStopWord(word)};
}

void SearchServer::SortUnique(vector<string_view> &vector) const
{
    sort(execution::par, vector.begin(), vector.end());
    auto last_unique = unique(execution::par, vector.begin(), vector.end());
    vector.erase(last_unique, vector.end());
}

SearchServer::Query SearchServer::ParseQuery(string_view text, bool sort_request) const
{
    SearchServer::Query result;
    vector<string_view> words = SplitIntoWordsView(text);

    for (string_view word : words)
    {
        auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                result.minus_words.push_back(move(query_word.data));
            }
            else
            {
                result.plus_words.push_back(move(query_word.data));
            }
        }
    }
    if (sort_request)
    {
        SortUnique(result.minus_words);
        SortUnique(result.plus_words);
    }

    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const
{
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void AddDocument(SearchServer &search_server, int document_id, string_view document,
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

void FindTopDocuments(const SearchServer &search_server, string_view raw_query)
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

void MatchDocuments(const SearchServer &search_server, string_view query)
{
    try
    {
        cout << "Matching for request: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index)
        {
            auto it = search_server.begin();
            advance(it, index);
            const int document_id = *it;
            auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception &e)
    {
        cout << "Error in matchig request "s << query << ": "s << e.what() << endl;
    }
    LogDuration guard("Match document", cerr);
}