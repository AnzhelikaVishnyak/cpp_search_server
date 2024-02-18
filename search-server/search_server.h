#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <set>
#include <map>
#include <stdexcept>
#include <cmath>
#include <execution>
#include <string_view>
#include <deque>
#include "string_processing.h"
#include "document.h"
#include "concurrent_map.h"


const int MAX_RESULT_DOCUMENT_COUNT = 5;

using matched_data_and_status_t = std::tuple<std::vector<std::string_view>, DocumentStatus>;

class SearchServer {
public:

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words);

    explicit SearchServer(const std::string& stop_words);

    explicit SearchServer(std::string_view stop_words);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status,
        const std::vector<int>& ratings);

    template <typename Predicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query,
        Predicate predicate) const;
    template <typename ExecutionPolicy, typename Predicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query,
        Predicate predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus set_status) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus set_status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query) const;

    int GetDocumentCount() const;

    matched_data_and_status_t MatchDocument(std::string_view raw_query,
        int document_id) const;
    matched_data_and_status_t MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query,
        int document_id) const;
    matched_data_and_status_t MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query,
        int document_id) const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);



private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    std::deque<std::string> storage_;
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_; //word - id - frequency 
    std::map<int, DocumentData> documents_;
    std::set<int> document_id_;
    std::map<int, std::map<std::string_view, double>> id_to_word_to_document_freqs_; // id - word - frequency 

    static bool IsValidWord(std::string_view word);

    bool IsStopWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text, bool delete_copy = true) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename Predicate>
    std::vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const;
    template <typename ExecutionPolicy, typename Predicate>
    std::vector<Document> FindAllDocuments(const ExecutionPolicy& policy, const Query& query, Predicate predicate) const;
};

template <typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words) {

    for (const std::string& word : stop_words) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Stop-words contain invalid characters"s);
        }

        stop_words_.insert(word);
    }
}

template <typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
    Predicate predicate) const {

    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, predicate);

    const double epsilon = 1e-6;
    std::sort(matched_documents.begin(), matched_documents.end(),
        [epsilon](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < epsilon) {
                return lhs.rating > rhs.rating;
            }
            return lhs.relevance > rhs.relevance;
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename ExecutionPolicy, typename Predicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query,
    Predicate predicate) const {

    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, predicate);

    const double epsilon = 1e-6;
    std::sort(matched_documents.begin(), matched_documents.end(),
        [epsilon](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < epsilon) {
                return lhs.rating > rhs.rating;
            }
            return lhs.relevance > rhs.relevance;
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;

}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus set_status) const {
    return SearchServer::FindTopDocuments(policy, raw_query, [set_status](int document_id, DocumentStatus status, int rating) {return status == set_status; });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query) const {
    DocumentStatus set_status = DocumentStatus::ACTUAL;
    return SearchServer::FindTopDocuments(policy, raw_query, set_status);
}

template <typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Predicate predicate) const {
    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document = documents_.at(document_id);
            if (predicate(document_id, document.status, document.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename ExecutionPolicy, typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy& policy, const Query& query, Predicate predicate) const {
    ConcurrentMap<int, double> document_to_relevance(100);
    std::for_each(policy,
        query.plus_words.begin(), query.plus_words.end(),
        [this, &predicate, &document_to_relevance](const std::string_view word) {
            if (this->word_to_document_freqs_.count(word) != 0) {
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : this->word_to_document_freqs_.at(word)) {
                    const auto& document = this->documents_.at(document_id);
                    if (predicate(document_id, document.status, document.rating)) {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }
            }
        });

    std::for_each(policy,
        query.minus_words.begin(), query.minus_words.end(),
        [this, &predicate, &document_to_relevance](const std::string_view word) {
            if (this->word_to_document_freqs_.count(word) != 0) {
                for (const auto [document_id, _] : this->word_to_document_freqs_.at(word)) {
                    document_to_relevance.Erase(document_id);
                }
            }
        });

    auto final_map = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    matched_documents.reserve(final_map.size());
    for (const auto [document_id, relevance] : final_map) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}