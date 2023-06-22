#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(
    const SearchServer &search_server,
    const vector<string> &queries)
{
    vector<vector<Document>> documents_lists(queries.size());
    transform(execution::par, queries.begin(), queries.end(), documents_lists.begin(),
              [&search_server](const string &query)
              { return search_server.FindTopDocuments(query); });
    return documents_lists;
}

vector<Document> ProcessQueriesJoined(
    const SearchServer &search_server,
    const vector<string> &queries)
{
    vector<Document> docs;

    for (auto &vector_of_docs : ProcessQueries(search_server, queries))
    {
        docs.insert(docs.end(), vector_of_docs.begin(), vector_of_docs.end());
    }
    return docs;
}