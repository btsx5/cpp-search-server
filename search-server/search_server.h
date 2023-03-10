#pragma once
#include "document.h"
#include "string_processing.h"
#include "paginator.h"
#include "concurrent_map.h"
#include <set>
#include <algorithm>
#include <string>
#include <map>
#include <list>
#include <stdexcept>
#include <iostream>
#include <execution>
#include <type_traits>

constexpr double EPSILON = 1e-6;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
using namespace std::literals;

class SearchServer {
public:
    template<typename TypeStop>
    explicit SearchServer(const TypeStop& stopwords);

    explicit SearchServer(const std::string& stopwords);

    explicit SearchServer(const std::string_view& stopwords);

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status,
                     const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

    template <typename DocumentPredicate, class Execution>
    std::vector<Document> FindTopDocuments(Execution&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const;

    template <class Execution>
    std::vector<Document> FindTopDocuments(Execution&& policy, const std::string_view& raw_query, DocumentStatus status) const;

    template <class Execution>
    std::vector<Document> FindTopDocuments(Execution&& policy, const std::string_view& raw_query) const;

    size_t GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string_view& raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view& raw_query, int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template<class Execution>
    void RemoveDocument(Execution&& policy, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string words;
    };

    std::set<int> documents_ids_;
    std::set<std::string, std::less<>> stop_words_;
    std::map<int, std::map<std::string_view , double>> words_freq_;
    std::map<std::string_view, double> empty_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;

    bool IsStopWord(const std::string_view& word) const;

    static bool IsValidWord(const std::string_view& word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view& text, bool policy_par = false) const;

    double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

    template<typename Predicate>
    std::vector<Document> FindAllDocuments(std::execution::sequenced_policy, const Query& query, Predicate predicate) const;

    template<typename Predicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const Query& query, Predicate predicate) const;

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
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate, class Execution>
std::vector<Document> SearchServer::FindTopDocuments(Execution&& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
    sort(policy, matched_documents.begin(), matched_documents.end(),
         [](const Document& lhs, const Document& rhs) {
             if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
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

template <class Execution>
std::vector<Document> SearchServer::FindTopDocuments(Execution&& policy, const std::string_view& raw_query, DocumentStatus status) const {
    auto lambda = [status](int document_id, DocumentStatus status_lambda, int rating) {
        return status_lambda == status;
    };
    return  FindTopDocuments(policy, raw_query, lambda);
}

template <class Execution>
std::vector<Document> SearchServer::FindTopDocuments(Execution&& policy, const std::string_view& raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template<typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::sequenced_policy, const Query& query, Predicate predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view& word : query.plus_words) {
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

    for (const std::string_view& word : query.minus_words) {
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

template<typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const Query& query, Predicate predicate) const {
    ConcurrentMap<int, double> document_to_relevance(100);

    for_each(std::execution::par,
             query.plus_words.begin(),
             query.plus_words.end(),
             [&] (const std::string_view word) {
                 const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                 for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                     const auto& documentdata = documents_.at(document_id);
                     if (predicate(document_id, documentdata.status, documentdata.rating)) {
                         document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                     }
                 }
             });

    for_each(std::execution::par,
             query.minus_words.begin(),
             query.minus_words.end(),
             [&] (const std::string_view word) {
                 if (word_to_document_freqs_.count(word) > 0) {
                     for (const auto [document_id, _]: word_to_document_freqs_.at(word)) {
                         document_to_relevance.Erase(document_id);
                     }
                 }
             });

    std::map<int, double> ordinaryMap = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : ordinaryMap) {
        matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template<class Execution>
void SearchServer::RemoveDocument(Execution&& policy, int document_id) {
    if (!documents_ids_.count(document_id)) {
        return;
    }
    documents_ids_.erase(document_id);
    documents_.erase(document_id);
    auto& toErase = words_freq_.at(document_id);
    std::vector<const std::string*> words(toErase.size());
    std::transform(toErase.begin(),
                   toErase.end(),
                   words.begin(),
                   [](const auto& x) {
                       return &x.first;
                   });
    std::for_each(policy,
                  words.begin(),
                  words.end(),
                  [&](auto key) {
                      word_to_document_freqs_.at(*key).erase(document_id);
                  });
    words_freq_.erase(document_id);
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