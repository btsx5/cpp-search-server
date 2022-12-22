#include "search_server.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <numeric>

SearchServer::SearchServer (const std::string& stopwords)
        : SearchServer(SplitIntoWords(stopwords))   {
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status,
                 const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Incorrect ID " + std::to_string(document_id));
    }
    if (documents_.count(document_id)) {
        throw std::invalid_argument("ID is already in server " + std::to_string(document_id));
    }

    const std::vector<std::string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / static_cast<double>(words.size());
    documents_ids_.push_back(document_id);

    for (const std::string &word: words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }

    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    auto lambda = [status](int document_id, DocumentStatus status_lambda, int rating) { return status_lambda == status ;};
    return  FindTopDocuments(raw_query, lambda )
            ;}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

size_t SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;

    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }

    return std::tuple {matched_words, documents_.at(document_id).status};
}

int SearchServer::GetDocumentId(int index) const {
    return documents_ids_.at(index);
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument( "Incorrect symbol in document text : " + word);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        if (text == "-"s) {
            throw std::invalid_argument("Incorrect minus word query (empty)");
        }
        if (text[1] == '-') { throw std::invalid_argument("Incorrect minus word format (double '-')");
        }
        is_minus = true;
        text = text.substr(1);
    }
    return {text, is_minus};
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    for (const std::string& word : SplitIntoWordsNoStop(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (query_word.is_minus) {
            query.minus_words.insert(query_word.data);
        } else {
            query.plus_words.insert(query_word.data);
        }
    }

    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(static_cast<double>(GetDocumentCount()) * 1.0 / static_cast<double>(word_to_document_freqs_.at(word).size()));
}

std::ostream& operator<<(std::ostream& os, const Document& v) {
    os<<"{ "s;
    os<<"document_id = "<<v.id <<", relevance = " << v.relevance<< ", rating = " << v.rating;
    os<<" }"s;
    return os;
}