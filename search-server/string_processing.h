#pragma once
#include <vector>
#include <string>
#include <set>
#include <algorithm>
std::vector<std::string> SplitIntoWords(const std::string& text);

bool IsValidWord(const std::string& word);

template<typename TypeStop>
std::set<std::string> SetStopWords(const TypeStop& stopwords) {
    std::set<std::string> stop_words;
    stop_words.insert(stopwords.begin(), stopwords.end());
    return stop_words;
}