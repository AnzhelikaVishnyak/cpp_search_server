#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words) {
    if (!SearchServer::IsValidWord(stop_words)) {
        throw std::invalid_argument("Stop-words contain invalid characters"s);
    }
    for (std::string_view word : SplitIntoWords(stop_words)) {
        stop_words_.insert(static_cast<std::string>(word));
    }
}

SearchServer::SearchServer(std::string_view stop_words) {
    if (!SearchServer::IsValidWord(stop_words)) {
        throw std::invalid_argument("Stop-words contain invalid characters"s);
    }
    for (std::string_view word : SplitIntoWords(stop_words)) {
        stop_words_.insert(static_cast<std::string>(word));
    }
}


void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings) {

    if (document_id < 0) {
        throw std::invalid_argument("The document ID cannot be negative"s);
    }

    if (document_id_.count(document_id)) {
        throw std::invalid_argument("A document with this id has already been added"s);
    }


    if (!SearchServer::IsValidWord(document)) {
        throw std::invalid_argument("The text of the document contains invalid characters"s);
    }

    document_id_.insert(document_id);
    storage_.emplace_back(document);


    const std::vector<std::string_view> words = SearchServer::SplitIntoWordsNoStop(storage_.back());
    const double inv_word_count = 1.0 / words.size();
    for (std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        id_to_word_to_document_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ SearchServer::ComputeAverageRating(ratings), status });
}


std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus set_status) const {
    return SearchServer::FindTopDocuments(raw_query, [set_status](int document_id, DocumentStatus status, int rating) {return status == set_status; });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    DocumentStatus set_status = DocumentStatus::ACTUAL;
    return SearchServer::FindTopDocuments(raw_query, set_status);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

matched_data_and_status_t SearchServer::MatchDocument(std::string_view raw_query,
    int document_id) const {

    if (!SearchServer::IsValidWord(raw_query)) {
        throw std::invalid_argument("The request text contains invalid characters"s);
    }

    if (!document_id_.count(document_id)) {
        throw std::out_of_range("The request document_id is out of range"s);
    }

    const Query query = SearchServer::ParseQuery(raw_query);

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return { {}, documents_.at(document_id).status };
        }
    }
    std::vector<std::string_view> matched_words;
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }


    return { matched_words, documents_.at(document_id).status };
}

matched_data_and_status_t SearchServer::MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query,
    int document_id) const {

    if (!document_id_.count(document_id)) {
        throw std::out_of_range("The request document_id is out of range"s);
    }

    const Query query = SearchServer::ParseQuery(raw_query, false);

    if (std::any_of(std::execution::par, query.minus_words.begin(),
        query.minus_words.end(),
        [this, document_id](std::string_view minus_word) {
            return this->word_to_document_freqs_.count(minus_word) && this->word_to_document_freqs_.at(minus_word).count(document_id); })) {
        return { {}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());

    auto copy_last = std::copy_if(std::execution::par,
        query.plus_words.begin(),
        query.plus_words.end(),
        matched_words.begin(), [this, document_id](std::string_view plus_word) {
            return this->word_to_document_freqs_.count(plus_word) && this->word_to_document_freqs_.at(plus_word).count(document_id); });


    std::sort(matched_words.begin(), copy_last);
    auto last = std::unique(matched_words.begin(), copy_last);
    matched_words.erase(last, matched_words.end());


    return { matched_words, documents_.at(document_id).status };
}

matched_data_and_status_t SearchServer::MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query,
    int document_id) const {
    return SearchServer::MatchDocument(raw_query, document_id);
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_id_.cbegin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_id_.cend();
}

bool SearchServer::IsValidWord(std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}


SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word is invalid");
    }

    return { word, is_minus, IsStopWord(word) };
}


SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool delete_copy) const {

    if (!SearchServer::IsValidWord(text)) {
        throw std::invalid_argument("The request text contains invalid characters"s);
    }

    SearchServer::Query query;
    auto splited_text = SplitIntoWords(text);

    query.plus_words.reserve(splited_text.size());
    query.minus_words.reserve(splited_text.size());

    for (std::string_view word : splited_text) {
        const QueryWord query_word = SearchServer::ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }


    if (delete_copy) {
        {
            std::sort(query.minus_words.begin(), query.minus_words.end());
            auto last = std::unique(query.minus_words.begin(), query.minus_words.end());
            query.minus_words.erase(last, query.minus_words.end());
        }
        {
            std::sort(query.plus_words.begin(), query.plus_words.end());
            auto last = std::unique(query.plus_words.begin(), query.plus_words.end());
            query.plus_words.erase(last, query.plus_words.end());
        }

    }

    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (!id_to_word_to_document_freqs_.count(document_id)) {
        static const std::map<std::string_view, double> void_map = {};
        return void_map;
    }
    return id_to_word_to_document_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    if (!document_id_.count(document_id)) {
        return;
    }

    auto all_words_in_document = SearchServer::GetWordFrequencies(document_id);
    for (const auto [word, freq] : all_words_in_document) {
        word_to_document_freqs_[word].erase(document_id);
    }

    documents_.erase(document_id);

    document_id_.erase(document_id);

    id_to_word_to_document_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {
    if (!document_id_.count(document_id)) {
        return;
    }

    std::vector<const std::string_view*> words(SearchServer::GetWordFrequencies(document_id).size());
    std::transform(std::execution::par, id_to_word_to_document_freqs_.at(document_id).begin(),
        id_to_word_to_document_freqs_.at(document_id).end(),
        words.begin(),
        [](auto const& word) {return &word.first; });
    std::for_each(std::execution::par, words.begin(), words.end(), [this, document_id](const std::string_view* word) {this->word_to_document_freqs_[*word].erase(document_id); });

    documents_.erase(document_id);

    document_id_.erase(document_id);

    id_to_word_to_document_freqs_.erase(document_id);
}


void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    SearchServer::RemoveDocument(document_id);
}
