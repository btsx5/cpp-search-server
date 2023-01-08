#pragma once
#include "document.h"
#include "string_processing.h"
#include "log_duration.h"
#include <set>
#include <algorithm>
#include <string>
#include <map>
#include <stdexcept>
#include <iostream>

constexpr double EPSILON = 1e-6;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
using namespace std::literals;

class SearchServer {
public:
    template<typename TypeStop>
    explicit SearchServer(const TypeStop& stopwords);

    explicit SearchServer(const std::string& stopwords);

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    void AddDocument(int document_id, const std::string& document, DocumentStatus status,
                     const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    size_t GetDocumentCount() const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::set<int> documents_ids_;
    std::set<std::string> stop_words_;
    std::map<int, std::map<std::string, double>> words_freq_;
    std::map<std::string, double> empty_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;

    bool IsStopWord(const std::string& word) const;

    static bool IsValidWord(const std::string& word);

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
    };

    QueryWord ParseQueryWord(std::string text) const;

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    Query ParseQuery(const std::string& text) const;

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template<typename Predicate>
    std::vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const;

};

template<typename TypeStop>
SearchServer::SearchServer(const TypeStop& stopwords)
        : stop_words_(SetStopWords(stopwords))   {
    for (const std::string& word : stop_words_) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Incorrect symbol in stop words"s);
        }
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
         [](const Document& lhs, const Document& rhs) {
             if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

template<typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Predicate predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto documentdata = documents_.at(document_id);
            if (predicate(document_id, documentdata.status, documentdata.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& os, std::map<Key, Value> source) {
    bool is_first = true;
    os<<"{"s;
    for (const auto& [key, value] : source) {
        if (is_first) { os<< key << ": " << value;}
        else {
            os<<", "s <<key << ": " << value ;
        }
        is_first = false;
    }
    os<<"}"s;
    return os;
}

std::ostream& operator<<(std::ostream& os, const Document& v);