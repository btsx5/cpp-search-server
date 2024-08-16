#include "search_server.h"
#include "process_queries.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <numeric>

SearchServer::SearchServer(const std::string& stopwords)
        : SearchServer(std::string_view(stopwords)) {
}

SearchServer::SearchServer (const std::string_view& stopwords)
        : SearchServer(SplitIntoWords(stopwords))   {
}

std::set<int>::const_iterator SearchServer::begin() const {
    return documents_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return documents_ids_.end();
}

void SearchServer::AddDocument(int document_id, const std::string_view& document, DocumentStatus status,
                               const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Incorrect ID " + std::to_string(document_id));
    }
    if (documents_.count(document_id)) {
        throw std::invalid_argument("ID is already in server " + std::to_string(document_id));
    }

    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status, std::string(document)});
    const std::vector<std::string_view> words = SplitIntoWordsNoStop(documents_[document_id].words);
    const double inv_word_count = 1.0 / static_cast<double>(words.size());
    documents_ids_.insert(document_id);

    for (const std::string_view word: words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        words_freq_[document_id][word] +=inv_word_count;
    }

}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const {
    auto lambda = [status](int document_id, DocumentStatus status_lambda, int rating) {
        return status_lambda == status ;
    };
    return  FindTopDocuments(raw_query, lambda);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

size_t SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view& raw_query, int document_id) const {
    if ((document_id < 0) || !(documents_.count(document_id))) {
        throw std::invalid_argument("Invalid document ID"s);
    }

    const Query query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;
    for (const std::string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            return std::tuple {matched_words, documents_.at(document_id).status};
        }
    }

    for (const std::string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return std::tuple {matched_words, documents_.at(document_id).status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy, const std::string_view& raw_query, int document_id) const {
    if ((document_id < 0) || !(documents_.count(document_id))) {
        throw std::invalid_argument("Invalid document ID"s);
    }

    const Query query = ParseQuery(raw_query, true);
    std::vector<std::string_view> matched_words(query.plus_words.size());
    auto lambdaCheck = [&](const std::string_view& word) {
        return word_to_document_freqs_.at(word).count(document_id);
    };

    if (std::any_of(query.minus_words.begin(),
                    query.minus_words.end(),
                    lambdaCheck)) {
        matched_words.clear();
        return std::tuple {matched_words, documents_.at(document_id).status};
    }
    auto it = std::copy_if(std::execution::par,
                           query.plus_words.begin(),
                           query.plus_words.end(),
                           matched_words.begin(),
                           lambdaCheck);
    matched_words.resize(std::distance(matched_words.begin(), it));
    std::sort(matched_words.begin(), matched_words.end());
    matched_words.erase(std::unique(matched_words.begin(),
                                    matched_words.end()),
                                    matched_words.end());

    return std::tuple {matched_words, documents_.at(document_id).status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, const std::string_view& raw_query, int document_id) const {
    if ((document_id < 0) || !(documents_.count(document_id))) {
        throw std::invalid_argument("Invalid document ID"s);
    }

    return MatchDocument(raw_query, document_id);
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (!documents_ids_.count(document_id)) {
        return empty_;
    }
    return words_freq_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {

    for (auto [word, freq] : words_freq_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    words_freq_.erase(document_id);
    documents_ids_.erase(document_id);
    documents_.erase(document_id);
}

bool SearchServer::IsStopWord(const std::string_view& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string_view& word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view& text) const {
    std::vector<std::string_view> words;
    for (const std::string_view& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument( "Incorrect symbol in document text : ");
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

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
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

SearchServer::Query SearchServer::ParseQuery(const std::string_view& text, bool policy_par) const {
    Query query;

    for (const std::string_view& word : SplitIntoWordsNoStop(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (query_word.is_minus) {
            query.minus_words.push_back(query_word.data);
        } else {
            query.plus_words.push_back(query_word.data);
        }
    }

    if (policy_par) {
        return query;
    }

    std::sort(query.minus_words.begin(),
              query.minus_words.end());
    std::sort(query.plus_words.begin(),
              query.plus_words.end());
    query.plus_words.erase(std::unique(query.plus_words.begin(),
                                       query.plus_words.end()),
                                       query.plus_words.end());
    query.minus_words.erase(std::unique(query.minus_words.begin(),
                                        query.minus_words.end()),
                                        query.minus_words.end());

    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view& word) const {
    return log(static_cast<double>(GetDocumentCount()) * 1.0 / static_cast<double>(word_to_document_freqs_.at(word).size()));
}

std::ostream& operator<<(std::ostream& os, const Document& v) {
    os<<"{ "s;
    os<<"document_id = "<<v.id <<", relevance = " << v.relevance<< ", rating = " << v.rating;
    os<<" }"s;
    return os;
}