#include "remove_duplicates.h"

#include <string>
#include <map>
#include <algorithm>

using namespace std;

void RemoveDuplicates(SearchServer &search_server)
{
    map<set<string>, int> words_to_id;
    set<int> duplicate_ids;
    int current_id;
    int max_id;
    int min_id;
    for (const int id : search_server)
    {
        map<string, double> word_freqs = search_server.GetWordFrequencies(id);
        set<string> words_to_compare;
        for (const auto &word : word_freqs)
        {
            words_to_compare.insert(word.first);
        }
        if (words_to_id.count(words_to_compare))
        {
            current_id = words_to_id.at(words_to_compare);
            max_id = max(current_id, id);
            duplicate_ids.insert(max_id);
            min_id = min(current_id, id);
            words_to_id[words_to_compare] = min_id;
        }
        else
        {
            words_to_id.insert(pair{words_to_compare, id});
        }
    }
    for (const int id : duplicate_ids)
    {
        cout << "Found duplicate document id " << id << endl;
        search_server.RemoveDocument(id);
    }
}