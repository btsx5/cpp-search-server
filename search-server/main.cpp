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

static bool IsValidWord(const string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
            : id(id)
            , relevance(relevance)
            , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

template<typename TypeStop>
set<string> SetStopWords(const TypeStop& stopwords) {
    set<string> stop_words;
    stop_words.insert(stopwords.begin(), stopwords.end());
    return stop_words;
}

class SearchServer {
public:
    template<typename TypeStop>
    explicit SearchServer(const TypeStop& stopwords)
            : stop_words_(SetStopWords(stopwords))   {
       for (const string& word : stop_words_) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Incorrect symbol in stop words"s);
            }
        }
    }

    explicit SearchServer(const string& stopwords)
            : SearchServer(SplitIntoWords(stopwords))   {
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("Incorrect ID");
        }
        if (documents_.count(document_id)) {
            throw invalid_argument("ID is already in server");
        }
        if (!IsValidWord(document)) {
            throw invalid_argument("Incorrect symbol in document text");
        }

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        documents_ids_.push_back(document_id);

        for (const string &word: words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }

        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);

        if (query.minus_words.count(""s)) {
            throw invalid_argument("Incorrect minus word query (empty)");
        }
        if (!IsDoubleMinus(query)) {
            throw invalid_argument("Incorrect minus word query (--)");
        }

        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        auto lambda = [status](int document_id, DocumentStatus status_lambda, int rating) { return status_lambda == status ;};
        return  FindTopDocuments(raw_query, lambda )
                ;}

    vector<Document>FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    size_t GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;

        if (query.minus_words.count(""s)) {
            throw invalid_argument("Incorrect minus word query (empty)");
        }
        if (!IsDoubleMinus(query)) {
            throw invalid_argument("Incorrect minus word query (--)");
        }

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

        return tuple {matched_words, documents_.at(document_id).status};
    }

    int GetDocumentId(int index) const {
        return documents_ids_.at(index);
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    
    vector<int> documents_ids_;
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
        if (!IsValidWord(text)) {
            throw invalid_argument("Incorrect symbol in query");
        }
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

    static bool IsDoubleMinus(const Query& query) {
        for (const string& minus_word : query.minus_words) {
            if (minus_word[0] == '-') { return false;}
        }
        return true;
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
    }
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
        SearchServer server("the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет, что поисковая система правильно добавляет новые документы
void TestAddingDocument() {
    SearchServer server("in the"s);
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
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("cat -city"s).empty(), "Minus words must be excluded from documents"s);
    }
}

// Тест проверяет, что в поисковой системе правильно находятся документы соответствующие поисковому запросу
void TestMatchingDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("cat", 42);
        ASSERT_EQUAL(words[0],"cat"s);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("-cat", 42);
        ASSERT(words.empty());
    }
}

// Тест проверяет, что поисковая система правильно сортирует документы по релевантности
void TestRelevanceSort() {
    SearchServer server("in the"s);
    server.AddDocument(1, "a f c d"s, DocumentStatus::ACTUAL,{1, 2, 3});
    server.AddDocument(2, "a b c d f"s, DocumentStatus::ACTUAL,{1, 2, 3});
    server.AddDocument(3, "a e s f"s, DocumentStatus::ACTUAL,{1, 2, 3});
    const auto top_doc = server.FindTopDocuments("a b c");
    ASSERT(top_doc[0].relevance> top_doc[1].relevance);
    ASSERT(top_doc[1].relevance> top_doc[2].relevance);
}

// Тест проверяет, что поисковая система правильно вычисляет средний рейтинг документа.
void TestComputeAverageRating() {
    SearchServer server("in the"s);
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
    SearchServer server("in the"s);
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
    SearchServer server("in the"s);
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
    SearchServer server("in the"s);
    server.AddDocument(1, "a b c d"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(2, "e b e f"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(3, "z x v n"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto top_docs = server.FindTopDocuments("e z b");
    ASSERT(top_docs[0].relevance - 0.6507 < EPSILON);
    ASSERT(top_docs[1].relevance - 0.2747 < EPSILON);
    ASSERT(top_docs[2].relevance - 0.1014 < EPSILON);
}

void TestAddDocumentsExeption() {
    try {
        SearchServer server("test_stop_words"s);
        server.AddDocument(12, "cat int the forest"s, DocumentStatus::ACTUAL, {1, 2, 3});
        server.AddDocument(12, "dog out of woods"s, DocumentStatus::ACTUAL, {1, 2, 3});
    }
    catch (const invalid_argument &error) {
        cerr << error.what() << endl;
    }
    catch (...) {
        cout << "exeption?"s;
    }

    try {
        SearchServer server("test_stop_words"s);
        server.AddDocument(-2, "cat int the forest"s, DocumentStatus::ACTUAL, {1, 2, 3});
    }
    catch (const invalid_argument &error) {
        cerr << error.what() << endl;
    }
    catch (...) {
        cout << "exeption?"s;
    }

    try {
        SearchServer server("test_stop_words"s);
        server.AddDocument(1, "cat int the forest \n"s, DocumentStatus::ACTUAL, {1, 2, 3});
    }
    catch (const invalid_argument &error) {
        cerr << error.what() << endl;
    }
    catch (...) {
        cout << "exeption?"s;
    }

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
    TestAddDocumentsExeption();
}