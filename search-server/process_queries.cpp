#include "process_queries.h"
#include <numeric>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> queriesResult(queries.size());

    std::transform(std::execution::par,
        queries.begin(), queries.end(),
        queriesResult.begin(),
        [&search_server](std::string query) {
            return search_server.FindTopDocuments(query);
        }
    );

    return queriesResult;
}

std::list<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    auto topDocuments = ProcessQueries(search_server, queries);
    std::list<Document> queriesResult;
    for (auto x : topDocuments) {
        for (auto z : x) {
            queriesResult.push_back(z);
        }
    }
    return queriesResult;
}