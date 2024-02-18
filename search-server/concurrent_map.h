#pragma once

#include <map>
#include <vector>
#include <mutex>
#include <string>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        Access(const Key& key, typename ConcurrentMap::LittleMap& little_map)
            : guard(little_map.little_map_mutex), ref_to_value(little_map.little_map[key]) {}

        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

    };

    explicit ConcurrentMap(size_t bucket_count) : all_maps_(bucket_count) {}

    Access operator[](const Key& key) {
        int index = static_cast<uint64_t>(key) % all_maps_.size();
        ConcurrentMap::LittleMap& little_map = all_maps_[index];
        return { key, little_map };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;

        for (ConcurrentMap::LittleMap& _map : all_maps_) {
            std::lock_guard g(_map.little_map_mutex);
            result.merge(_map.little_map);
        }

        return result;
    }

    auto Erase(const Key& key) {
        int index = static_cast<uint64_t>(key) % all_maps_.size();
        std::lock_guard g(all_maps_[index].little_map_mutex);
        return all_maps_[index].little_map.erase(key);
    }

private:

    struct LittleMap {
        std::mutex little_map_mutex;
        std::map<Key, Value> little_map;
    };

    std::vector<LittleMap> all_maps_;

};
