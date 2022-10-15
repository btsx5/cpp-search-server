#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

struct Document {
    int id;
    double relevance;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        vector<string> words = SplitIntoWordsNoStop(document);
        double TF = 1.0 / words.size(); // формула TF
        for (string& word : words) {
             word_to_document_freqs_[word][document_id] += TF; /* по ключу(word) , и под-ключу (document_id)
             составляем словарь с Word > ID , TF. высчитываем TF , если слово встречается второй раз, ++TF  */
        }
        ++document_count_;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                return lhs.relevance > rhs.relevance;
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    struct Query {
        set<string> pluswords_;
        set<string> minuswords_;
    };

    double document_count_ = 0;
    // map<string, set<int>> word_to_documents_;
    map<string, map<int, double>> word_to_document_freqs_; // Словарь - Слово , ID / TF
    set<string> stop_words_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const string& text) const {
        Query query_words;
        for (string word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-') {

                query_words.minuswords_.insert(word.substr(1));
            }
            else {
                query_words.pluswords_.insert(word);
            }
        }

        return query_words;
    }

    double CalcIDF(const string& word) const {
        return log(document_count_ / word_to_document_freqs_.at(word).size()); // высчитываем IDF
    }

    vector<Document> FindAllDocuments(const Query& query_words) const {
        map<int, double> document_to_relevance;
        vector<Document> matched_documents;
        for (const string& word : query_words.pluswords_) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double IDF = CalcIDF(word);
            for (const auto [doc_id, TF] : word_to_document_freqs_.at(word)) { // проходим по словарю Слово/id/tf 
                document_to_relevance[doc_id] += TF * IDF;                     // плюс словами , и по ключу (doc id)
                // формируем словарь с ID / Relevance
            }
        }

        for (const string& word : query_words.minuswords_) {                    // проходим по словарю , но уже минус словами
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [doc_id, TF] : word_to_document_freqs_.at(word)) { // если слово находим , удаляем по ключу из 
                // словаря ID/Relevance
                document_to_relevance.erase(doc_id);
            }
        }

        for (const auto [document_id, relevance] : document_to_relevance) {    // возвращаем конечный вектор /id/rel/
            matched_documents.push_back({ document_id, relevance });
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }
    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
            << "relevance = "s << relevance << " }"s << endl;
    }
}