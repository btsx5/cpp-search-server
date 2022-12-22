#pragma once
#include "read_input_functions.h"
#include <string>
#include <vector>

template<typename Iterator>
class IteratorRange {
public:
    IteratorRange()=default;

    IteratorRange(const Iterator begin, const Iterator end, std::size_t size)
            : begin_(begin),
              end_(end),
              size_(size)
    {
    }

    Iterator begin() const {
        return begin_;
    }

    Iterator end() const {
        return end_;
    }

    size_t size() const {
        return size_;
    }

private:
    Iterator begin_;
    Iterator end_;
    size_t size_;

};

template <typename Iterator>
class Paginator {
public:
    Paginator(const Iterator range_begin, const Iterator range_end, std::size_t size)
    {
        FillUp(range_begin, range_end, size);
    }

    void FillUp(const Iterator range_begin, const Iterator range_end, std::size_t size) {
        auto temp_range_begin=range_begin;
        while(temp_range_begin+size < range_end) {
            IteratorRange ItR(temp_range_begin, temp_range_begin+size, size);
            pages_.push_back(ItR);
            temp_range_begin += size;
        }
        IteratorRange ItR(temp_range_begin,range_end,size);
        pages_.push_back(ItR);
    }
    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    std::size_t size() const {
        return pages_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, std::size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template<typename Iterator>
std::ostream& operator<<(std::ostream& os, const IteratorRange<Iterator>& v) {
    for (auto page = v.begin(); page != v.end(); ++page) {
        os<<*page;
    }
    return os;
}

