#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(std::string_view text) {
    std::vector<std::string_view> words;
    text.remove_prefix(std::min(text.size(), text.find_first_not_of(" ")));
    const int64_t pos_end = text.npos;

    while (!text.empty()) {
        int64_t space = text.find(' ');
        words.push_back(space == pos_end ? text.substr(0) : text.substr(0, space));
        text.remove_prefix(std::min(text.size(), text.find_first_not_of(" ", space)));
    }

    return words;
}
