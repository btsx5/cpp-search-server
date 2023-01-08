#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
std::set<std::set<std::string>> unique_documents;
std::vector<int> ids_to_remove;
for (int id : search_server) {
    std::set<std::string> words;
    for (const auto& [word, ids] : search_server.GetWordFrequencies(id)) {
    words.insert(word);
    }
    if (unique_documents.count(words)) {
        ids_to_remove.push_back(id);
    } else {
        unique_documents.insert(words);
    }
}
if (!ids_to_remove.empty()) {
    for (int id: ids_to_remove) {
        std::cout << "Found duplicate document id " << id << std::endl;
        search_server.RemoveDocument(id);
    }
}
}