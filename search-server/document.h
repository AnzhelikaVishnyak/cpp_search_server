#pragma once
#include <iostream>

using namespace std::string_literals;

struct Document {
    int id = 0;
    double relevance = 0;
    int rating = 0;

    Document() = default;

    Document(const int _id, const double _relevance, const int _rating)
        : id(_id), relevance(_relevance), rating(_rating)
    {
    }
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

std::ostream& operator<<(std::ostream& output, const Document& document);
