#pragma once
#include <vector>
#include <string>
#include <set>
#include <algorithm>
std::vector<std::string_view> SplitIntoWords(const std::string_view& text);

template<typename TypeStop>
std::set<std::string, std::less<>> SetStopWords(const TypeStop& stopwords) {
    std::set<std::string, std::less<>> stop_words;
    stop_words.insert(stopwords.begin(), stopwords.end());
    return stop_words;
}