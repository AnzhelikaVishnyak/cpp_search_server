#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
	std::set < std::set<std::string_view>> set_words;
	std::set<int> duplicates_id;
	
	for (const int document_id : search_server) {
		const auto first_map = search_server.GetWordFrequencies(document_id);
		std::set<std::string_view> words;

		std::transform(first_map.begin(), first_map.end(), std::inserter(words, words.end()), [](decltype(first_map)::value_type const& pair) {return pair.first; });
				
		if (set_words.count(words)) {
			duplicates_id.insert(document_id);
		}
		else {
			set_words.insert(words);
		}
	}

	for (const int document_id : duplicates_id) {
		std::cout << "Found duplicate document id "s << document_id << std::endl;
		search_server.RemoveDocument(document_id);
	}
}