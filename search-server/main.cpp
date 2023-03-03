#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <stdexcept>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) 
    {
        CheckStopWords();
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text)) 
    {
        CheckStopWords();
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                                   const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("Попытка добавить документ с отрицательным id"s);
        }
        if (documents_.count(document_id) > 0) {
            throw invalid_argument("Попытка добавить документ c id ранее добавленного документа"s);
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            if (IsValidWord(word)) {
                word_to_document_freqs_[word][document_id] += inv_word_count;
            } else {
                throw invalid_argument("Наличие недопустимых символов в тексте добавляемого документа"s);
            }
        }
        documents_index_.emplace(GetDocumentCount(), document_id);
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        Query query;
        bool parse_result = ParseQuery(raw_query, query);
        if (parse_result) {
            auto matched_documents = FindAllDocuments(query, document_predicate);
            sort(matched_documents.begin(), matched_documents.end(),
                 [](const Document& lhs, const Document& rhs) {
                     if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                        return lhs.rating > rhs.rating;
                     } else {
                         return lhs.relevance > rhs.relevance;
                     }
                 });
            if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
                matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
            }
            return matched_documents;
        }
        throw invalid_argument("Ошибка в поисковом запросе"s);
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query, [status] (int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentId(int index) const {
        if (index < 0 || index >= GetDocumentCount()) {
            throw out_of_range("Индекс переданного документа выходит за пределы допустимого диапазона"s);
        } 
        return documents_index_.at(index);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        Query query;
        bool parse_result = ParseQuery(raw_query, query);
        if (parse_result) {
            vector<string> matched_words;
            for (const string& word : query.plus_words) {
                if (word_to_document_freqs_.count(word) == 0) {
                    continue;
                }
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.push_back(word);
                }
            }
            for (const string& word : query.minus_words) {
                if (word_to_document_freqs_.count(word) == 0) {
                    continue;
                }
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.clear();
                    break;
                }
            }
            tuple<vector<string>, DocumentStatus> result = {matched_words, documents_.at(document_id).status};
            return result;
        }
        throw invalid_argument("Ошибка в поисковом запросе"s);
    }

private:

    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

    void CheckStopWords() {
        for (const auto& word: stop_words_) {
                if (!IsValidWord(word)) {
                    throw invalid_argument("Стоп слово содержит недопустимый символ"s);
                }
            }
    }

    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    map<int, int> documents_index_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    [[nodiscard]] bool ParseQuery(const string& text, Query& query) const {
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Наличие недопустимых символов в запросе"s);
            }
            if (word[0] != '-') {
                query.plus_words.insert(word);
            }
            else {
                if (word[1] == '-' || word[1] == ' ' || word == "-") {
                    throw invalid_argument("Ошибка в поисковом запросе"s);
                }
                string minus_word = word.substr(1);
                query.minus_words.insert(minus_word);
            }
        }
        return true;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
                                      DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}
int main() {
    SearchServer search_server("\x12кот"s);
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, {1, 3, 2});
    const auto documents = search_server.FindTopDocuments("-пу\x12шистый"s);
    for (const Document& document : documents) {
        PrintDocument(document);
    }
}
