// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

// Тест проверяет, что документ находится по поисковому запросу, который содержит слова из документа

void TestAddDocumentAndSearchWordsInDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        ASSERT_EQUAL(server.GetDocumentCount(), 0);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}

// Тест проверяет, что документы, содержащие минус-слова поискового запроса, не включаются в результаты поиска

void TestMinusWords() {
        SearchServer server;
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
        server.AddDocument(24, "cat the home city"s, DocumentStatus::ACTUAL, {3, 3, 3});
        vector<Document> result = server.FindTopDocuments("-in cat"s);
        ASSERT_EQUAL(result.size(), 1);
        ASSERT(result[0].id == 24);
}

// Тест проверяет, что при матчинге документа по поисковому запросу возвращаются все слова из поискового запроса, присутствующие в документе

void TestMatching() {
    SearchServer server;
    server.SetStopWords("cat"s);
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    {
    auto [test, status] = server.MatchDocument("in the"s, 42);
    vector<string> result = {"in"s, "the"s};
    ASSERT_EQUAL(test, result);
    ASSERT(status == DocumentStatus::ACTUAL);
    }
    {
    auto [test, status] = server.MatchDocument("in -the"s, 42);
    vector<string> result = {};
    ASSERT_EQUAL(test, result);
    ASSERT(status == DocumentStatus::ACTUAL);
    }
    {
    auto [test, status] = server.MatchDocument("in the cat"s, 42);
    vector<string> result = {"in"s, "the"s};
    ASSERT_EQUAL(test, result);
    ASSERT(status == DocumentStatus::ACTUAL);
    }
}

// Тест проверяет, что возвращаемые при поиске документов результаты отсортированы в порядке убывания релевантности

void TestSortByRelevance() {
    {
        SearchServer server;
        server.AddDocument(24, "cat in the home city"s, DocumentStatus::ACTUAL, {3, 3, 3});
        server.AddDocument(25, "cat in the city city"s, DocumentStatus::ACTUAL, {3, 3, 3});
        server.AddDocument(26, "cat in the home home"s, DocumentStatus::ACTUAL, {3, 3, 3});
        vector<Document> result = server.FindTopDocuments("city"s);
        ASSERT_EQUAL(result.size(), 2);
        ASSERT((result[0].relevance > result[1].relevance));
    }
}

// Тест проверяет, что рейтинг добавленного документа равен среднему арифметическому оценок документа

void TestRatingCalculation() {
    {
        SearchServer server;
        server.AddDocument(20, "cat in the home city"s, DocumentStatus::ACTUAL, {3, 4, 10});
        vector<Document> result = server.FindTopDocuments("city"s);
        ASSERT_EQUAL(result.size(), 1);
        ASSERT((result[0].rating == (3 + 4 + 10) / 3));
    }
}

// Тест проверяет, что фильтрация результатов поиска происходит с использованием предиката, задаваемого пользователем

void TestPredicateFiltr() {
    SearchServer server;
    server.AddDocument(20, "cat in the home city"s, DocumentStatus::ACTUAL, {1, 1, 1});
    server.AddDocument(21, "cat in the home city"s, DocumentStatus::BANNED, {1, 1, 1});
    server.AddDocument(22, "cat in the home city"s, DocumentStatus::IRRELEVANT, {1, 1, 1});
    server.AddDocument(23, "cat in the home city"s, DocumentStatus::REMOVED, {1, 1, 1});
    const auto result = server.FindTopDocuments("city"s, [](int document_id, DocumentStatus status, int rating){return status == DocumentStatus::ACTUAL;});
    ASSERT_EQUAL(result.size(), 1);
    ASSERT(result[0].id == 20);
}

// Тест проверяет, что система корректно ищет документы с заданным статусом

void TestSearchDocumentsWithStatus() {
    SearchServer server;
    server.AddDocument(20, "cat in the home city"s, DocumentStatus::ACTUAL, {1, 1, 1});
    server.AddDocument(21, "cat in the home city"s, DocumentStatus::IRRELEVANT, {1, 1, 1});
    const auto result = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL); 
    ASSERT_EQUAL(result.size(), 1);
    ASSERT(result[0].id == 20);
    const auto result_2 = server.FindTopDocuments("cat"s, DocumentStatus::BANNED); 
    ASSERT_EQUAL(result_2.size(), 0);
}

// Тест проверяет, что система корректно вычисляет релевантность найденных документов

void TestRelevanceCalculation() {
    {
        SearchServer server;
        server.AddDocument(24, "cat in the home city"s, DocumentStatus::ACTUAL, {3, 3, 3});
        server.AddDocument(26, "cat in the home home"s, DocumentStatus::ACTUAL, {3, 3, 3});
        vector<Document> result = server.FindTopDocuments("city"s);
        ASSERT_EQUAL(result.size(), 1);
        ASSERT(abs((result[0].relevance - log(2.0 / 1.0) * (1.0 / 5.0))) < 1e-6);
    }
}

void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent();
    TestAddDocumentAndSearchWordsInDocument();
    TestMinusWords();
    TestMatching();
    TestSortByRelevance();
    TestRatingCalculation();
    TestPredicateFiltr();
    TestSearchDocumentsWithStatus();
    TestRelevanceCalculation();
}

// --------- Окончание модульных тестов поисковой системы -----------
