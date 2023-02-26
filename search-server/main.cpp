
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
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
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
        ASSERT(result[0].id == 24);
        ASSERT_EQUAL(result.size(), 1);
}

// Тест проверяет, что при матчинге документа по поисковому запросу возвращаются все слова из поискового запроса, присутствующие в документе

void TestMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        tuple<vector<string>, DocumentStatus> expectation = {{"in"s,"the"s},DocumentStatus::ACTUAL};
        tuple<vector<string>, DocumentStatus> expectation_with_minus_word = {{},DocumentStatus::ACTUAL};
        ASSERT((server.MatchDocument("in the"s, 42) == expectation));
        ASSERT((server.MatchDocument("in the -city"s, 42) == expectation_with_minus_word));
    }
}

// Тест проверяет, что возвращаемые при поиске документов результаты отсортированы в порядке убывания релевантности

void TestSortByRelevance() {
    {
        SearchServer server;
        server.AddDocument(24, "cat in the home city"s, DocumentStatus::ACTUAL, {3, 3, 3});
        server.AddDocument(25, "cat in the city city"s, DocumentStatus::ACTUAL, {3, 3, 3});
        server.AddDocument(26, "cat in the home home"s, DocumentStatus::ACTUAL, {3, 3, 3});
        vector<Document> expectation;
        expectation.push_back({25, log(3.0/2.0) * (2.0/5.0), 3});
        expectation.push_back({24, log(3.0/2.0) * (1.0/5.0), 3});
        vector<Document> result = server.FindTopDocuments("city"s);
        ASSERT((result[0].id == expectation[0].id));
        ASSERT((result[1].id == expectation[1].id));
        ASSERT_EQUAL(result.size(), 2);
    }
}

// Тест проверяет, что рейтинг добавленного документа равен среднему арифметическому оценок документа

void TestRatingCount() {
    {
        SearchServer server;
        server.AddDocument(20, "cat in the home city"s, DocumentStatus::ACTUAL, {3, 4, 10});
        vector<Document> result = server.FindTopDocuments("city"s);
        vector<Document> expectation;
        expectation.push_back({20, log(1.0/1.0) * (1.0/5.0), 5});
        ASSERT((result[0].rating == expectation[0].rating));
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
    ASSERT(result[0].id == 20);
    ASSERT_EQUAL(result.size(), 1);
}

// Тест проверяет, что система корректно ищет документы с заданным статусом

void TestSearchDocumentsWithStatus() {
    SearchServer server;
    server.AddDocument(20, "cat in the home city"s, DocumentStatus::ACTUAL, {1, 1, 1});
    server.AddDocument(21, "cat in the home city"s, DocumentStatus::IRRELEVANT, {1, 1, 1});
    const auto result = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL); 
    ASSERT(result[0].id == 20);
    ASSERT_EQUAL(result.size(), 1);
}

// Тест проверяет, что система корректно вычисляет релевантность найденных документов

void TestRelevanceCount() {
    {
        SearchServer server;
        server.AddDocument(24, "cat in the home city"s, DocumentStatus::ACTUAL, {3, 3, 3});
        server.AddDocument(26, "cat in the home home"s, DocumentStatus::ACTUAL, {3, 3, 3});
        vector<Document> expectation;
        expectation.push_back({24, log(2.0/1.0) * (1.0/5.0), 3});
        vector<Document> result = server.FindTopDocuments("city"s);
        ASSERT((result[0].relevance == expectation[0].relevance));
        ASSERT_EQUAL(result.size(), 1);
    }
}

void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent();
    TestAddDocumentAndSearchWordsInDocument();
    TestMinusWords();
    TestMatching();
    TestSortByRelevance();
    TestRatingCount();
    TestPredicateFiltr();
    TestSearchDocumentsWithStatus();
    TestRelevanceCount();
}

// --------- Окончание модульных тестов поисковой системы -----------
