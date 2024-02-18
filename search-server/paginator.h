#pragma once
#include <stdexcept>
#include "document.h"

using namespace std::string_literals;

template <typename Iterator>
class IteratorRange {
public:

    IteratorRange(const Iterator begin, const Iterator end)
        : begin_(begin), end_(end)
    {
    }

    Iterator begin() const {
        return begin_;
    }

    Iterator end() const {
        return end_;
    }

    int size() const {
        return distance(begin_, end_);
    }

private:
    Iterator begin_;
    Iterator end_;
};

template <typename Iterator>
class Paginator {
public:

    Paginator(Iterator begin, Iterator end, size_t page_size) {
        if (page_size == 0) {
            throw std::invalid_argument("The page size cannot be equal to 0"s);
        }
        int number_of_pages = 0;
        int result_size = distance(begin, end);
        if (result_size % page_size == 0) {
            number_of_pages = result_size / page_size;
        }
        else {
            number_of_pages = result_size / page_size + 1;
        }

        auto it = begin;
        for (auto i = 0; i < number_of_pages; ++i) {
            if (distance(it, end) < page_size) {
                pages_.push_back(IteratorRange(it, end));
                break;
            }
            else {
                pages_.push_back(IteratorRange(it, it + page_size));
                it += page_size;
            }
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    int size() const {
        return distance(pages_.begin(), pages_.end());
    }

private:

    std::vector<IteratorRange<Iterator>> pages_;

};

template <typename Iterator>
std::ostream& operator<<(std::ostream& output, const IteratorRange<Iterator>& iterator_range) {

    for (auto it = begin(iterator_range); it != end(iterator_range); ++it) {
        output << *it;
    }

    return output;
}

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
