#include "test_framework.h"

void AssertImpl(bool value, const std::string& expr_str, const std::string& func_name, const std::string& file_name, int line_number, const std::string& hint) {
    if (!value) {
        std::cerr << file_name <<"(" << line_number << "): ";
        std::cerr << func_name << ":";
        std::cerr << " ASSERT("s << expr_str << ") failed. " ;
        if (!hint.empty()) {
            std::cout << "Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
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
void TestExcludeMinusWordsFromTopDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("cat -city"s).empty(), "Minus words must be excluded from documents"s);
    }
}
void TestMatchingDocuments() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};
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
void TestRelevanceSort() {
    SearchServer server("in the"s);
    server.AddDocument(1, "a f c d"s, DocumentStatus::ACTUAL,{1, 2, 3});
    server.AddDocument(2, "a b c d f"s, DocumentStatus::ACTUAL,{1, 2, 3});
    server.AddDocument(3, "a e s f"s, DocumentStatus::ACTUAL,{1, 2, 3});
    const auto top_doc = server.FindTopDocuments("a b c");
    ASSERT(top_doc[0].relevance> top_doc[1].relevance);
    ASSERT(top_doc[1].relevance> top_doc[2].relevance);
}
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
    catch (const std::invalid_argument &error) {
        std::cerr << error.what() << std::endl;
    }
    catch (...) {
        std::cout << "exeption?"s;
    }

    try {
        SearchServer server("test_stop_words"s);
        server.AddDocument(-2, "cat int the forest"s, DocumentStatus::ACTUAL, {1, 2, 3});
    }
    catch (const std::invalid_argument &error) {
        std::cerr << error.what() << std::endl;
    }
    catch (...) {
        std::cout << "exeption?"s;
    }

    try {
        SearchServer server("test_stop_words"s);
        server.AddDocument(1, "cat int the forest\n"s, DocumentStatus::ACTUAL, {1, 2, 3});
    }
    catch (const std::invalid_argument &error) {
        std::cerr << error.what() << std::endl;
    }
    catch (...) {
        std::cout << "exeption?"s;
    }

}
void FindTopDocumentsExeption() {
    try {
        SearchServer server("test_stop_words"s);
        server.AddDocument(12, "cat int the forest"s, DocumentStatus::ACTUAL, {1, 2, 3});
        server.FindTopDocuments("cat -");
    }
    catch (const std::invalid_argument &error) {
        std::cerr << error.what() << std::endl;
    }
    catch (...) {
        std::cout << "exeption?"s;
    }
}
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