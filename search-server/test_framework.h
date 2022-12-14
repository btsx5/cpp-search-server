#pragma once
#include "search_server.h"

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cout << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& func_name, const std::string& file_name, int line_number, const std::string& hint);

#define ASSERT(expr) AssertImpl(expr, #expr, __FUNCTION__, __FILE__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(expr, #expr, __FUNCTION__, __FILE__, __LINE__, hint)

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();

// Тест проверяет, что поисковая система правильно добавляет новые документы
void TestAddingDocument();

// Тест проверяет, что поисковая система исключает минус-слова при добавлении документов
void TestExcludeMinusWordsFromTopDocumentContent();

// Тест проверяет, что в поисковой системе правильно находятся документы соответствующие поисковому запросу
void TestMatchingDocuments();

// Тест проверяет, что поисковая система правильно сортирует документы по релевантности
void TestRelevanceSort();

// Тест проверяет, что поисковая система правильно вычисляет средний рейтинг документа.
void TestComputeAverageRating();

// Тест проверяет, что поисковая система правильно фильтрует документы с использованием конкретного предиката.
void TestPredicateWork();

// Тест проверяет, что поисковая система правильно фильтрует документы по статусу.
void TestStatusFilterWork();

// Тест проверяет, что поисковая система правильно высчитывает релевантность.
void TestRelevanceCalc();

void TestAddDocumentsExeption();

void FindTopDocumentsExeption();

template <typename func_name>
void RunTestImpl(func_name test, const std::string& func_n  ) {
    test();
    std::cerr << func_n << " OK"s << std::endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer();
// --------- Окончание модульных тестов поисковой системы -----------
