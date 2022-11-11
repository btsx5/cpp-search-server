#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <sstream>
#include <cstdlib>
#include <iostream>

using namespace std;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double EPSILON = 1e-6;

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
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
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / static_cast<double>(words.size());
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template<typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      Predicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {

                 if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
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

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return  FindTopDocuments(raw_query, []([[maybe_unused]]int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; })
                ;}
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        auto lambda = [status](int document_id, DocumentStatus status_lambda, int rating) { return status_lambda == status ;};
        return  FindTopDocuments(raw_query, lambda )
                ;}

    size_t GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

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

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(static_cast<double>(GetDocumentCount()) * 1.0 / static_cast<double>(word_to_document_freqs_.at(word).size()));
    }

    template<typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                    {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

template <typename Key, typename Value>
ostream& operator<<(ostream& os, map<Key, Value> source) {
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

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& func_name, const string& file_name, int line_number, const string& hint) {
    if (!value) {
        cerr << file_name <<"(" << line_number << "): ";
        cerr << func_name << ":";
        cerr << " ASSERT("s << expr_str << ") failed. " ;
        if (!hint.empty()) {
            cout << "Hint: "s << hint;
        }
        cerr << endl;
        abort();
    } // Реализуйте тело функции Assert
}

#define ASSERT(expr) AssertImpl(expr, #expr, __FUNCTION__, __FILE__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(expr, #expr, __FUNCTION__, __FILE__, __LINE__, hint)

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
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет, что поисковая система правильно добавляет новые документы
void TestAddingDocument() {
    SearchServer server;
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL,{1, 2, 3});
    const auto found_docs = server.FindTopDocuments("cat");
    ASSERT_EQUAL(found_docs.size(), 1u);
    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL(doc0.id, 1);
    server.AddDocument(2, "cat in the city"s, DocumentStatus::BANNED,{1, 2, 3});
    server.AddDocument(3, "cat in the city"s, DocumentStatus::IRRELEVANT,{1, 2, 3});
    server.AddDocument(4, "cat in the city"s, DocumentStatus::REMOVED,{1, 2, 3});
    server.AddDocument(5, "dog in the cat city"s, DocumentStatus::ACTUAL,{1, 2, 3});
    const auto found_docs1 = server.FindTopDocuments("cat");
    const auto found_docs2 = server.FindTopDocuments("dog");
    ASSERT_EQUAL(found_docs1.size(), 2u);
    ASSERT_EQUAL(found_docs2.size(), 1u);
    const Document& doc1 = found_docs1[0];
    const Document& doc2 = found_docs1[1];
    const Document& doc3 = found_docs2[0];
    ASSERT_EQUAL(doc1.id, 1);
    ASSERT_EQUAL(doc2.id, 5);
    ASSERT_EQUAL(doc3.id,5);
}

// Тест проверяет, что поисковая система исключает минус-слова при добавлении документов
void TestExcludeMinusWordsFromTopDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("cat -city"s).empty(), "Minus words must be excluded from documents"s);
    }
}

// Тест проверяет, что в поисковой системе правильно вычисляется документы соответствующие поисковому запросу
void TestMatchingDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("cat", 42);
        ASSERT_EQUAL(words[0],"cat"s);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("-cat", 42);
        ASSERT(!words.size());
    }
}

// Тест проверяет, что поисковая система правильно сортирует документы по релевантности
void TestRelevanceSort() {
    SearchServer server;
    server.AddDocument(1, "a f c d"s, DocumentStatus::ACTUAL,{1, 2, 3});
    server.AddDocument(2, "a b c d f"s, DocumentStatus::ACTUAL,{1, 2, 3});
    server.AddDocument(3, "a e s f"s, DocumentStatus::ACTUAL,{1, 2, 3});
    const auto top_doc = server.FindTopDocuments("a b c");
    ASSERT_EQUAL(top_doc[0].id, 2);
    ASSERT_EQUAL(top_doc[1].id, 1);
    ASSERT_EQUAL(top_doc[2].id, 3);
}

// Тест проверяет, что поисковая система правильно вычисляет средний рейтинг документа.
void TestComputeAverageRating() {
    SearchServer server;
    server.AddDocument(1, "a f c d"s, DocumentStatus::ACTUAL,{0, 0, 0});
    server.AddDocument(2, "a b c d f"s, DocumentStatus::ACTUAL,{5, 2, 4});
    server.AddDocument(3, "a e s f"s, DocumentStatus::ACTUAL,{-30, -10, 0});
    const auto top_doc = server.FindTopDocuments("a b c");
    ASSERT_EQUAL(top_doc[0].rating, 3);
    ASSERT_EQUAL(top_doc[1].rating, 0);
    ASSERT_EQUAL(top_doc[2].rating, -13);
}

// Тест проверяет, что поисковая система правильно фильтрует документы с использованием конкретного предиката.
void TestPredicateWork() {
    SearchServer server;
    server.AddDocument(1, "a a c d"s, DocumentStatus::ACTUAL,{1, 2, 3});
    server.AddDocument(2, "a b c d f"s, DocumentStatus::BANNED,{1, 2, 3});
    server.AddDocument(3, "a e s f"s, DocumentStatus::IRRELEVANT,{1, 2, 3});
    server.AddDocument(4, "a f c d"s, DocumentStatus::REMOVED,{1, 2, 3});
    const auto top_docs = server.FindTopDocuments("a",[](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_EQUAL(top_docs.size(), 2u);
    ASSERT_EQUAL(top_docs[0].id, 2);
    ASSERT_EQUAL(top_docs[1].id, 4);
}

// Тест проверяет, что поисковая система правильно фильтрует документы по статусу.
void TestStatusFilterWork() {
    SearchServer server;
    server.AddDocument(5, "a b c d"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "a b c d f"s, DocumentStatus::BANNED, {1, 2, 3});
    server.AddDocument(3, "a e s f"s, DocumentStatus::IRRELEVANT, {1, 2, 3});
    server.AddDocument(1, "a f c d"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto top_docs = server.FindTopDocuments("a b", DocumentStatus::ACTUAL);
    ASSERT_EQUAL(top_docs.size(), 2);
    ASSERT_EQUAL(top_docs[0].id, 5);
    ASSERT_EQUAL(top_docs[1].id, 1);
    const auto top_docs_b = server.FindTopDocuments("a b", DocumentStatus::BANNED);
    ASSERT_EQUAL(top_docs_b.size(), 1);
    ASSERT_EQUAL(top_docs_b[0].id, 2);
}

// Тест проверяет, что поисковая система правильно высчитывает релевантность.
void TestRelevanceCalc() {
    SearchServer server;
    server.AddDocument(1, "a b c d"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "e b e f"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(3, "z x v n"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto top_docs = server.FindTopDocuments("e z b");
    ASSERT(top_docs[0].relevance - 0.6507 < 1e-6);
    ASSERT(top_docs[1].relevance - 0.2747 < 1e-6);
    ASSERT(top_docs[2].relevance - 0.1014 < 1e-6);
}

template <typename func_name>
void RunTestImpl(func_name test, const string& func_n  ) {
    test();
    cerr << func_n << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddingDocument);
    RUN_TEST(TestExcludeMinusWordsFromTopDocumentContent);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestRelevanceSort);
    RUN_TEST(TestComputeAverageRating);
    RUN_TEST(TestPredicateWork);
    RUN_TEST(TestStatusFilterWork);
    RUN_TEST(TestRelevanceCalc);
}
// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}