#pragma once
#include <future>
#include <map>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : buck_C(bucket_count), m_(uint64_t(bucket_count)) {
        for (size_t i = 0 ; i < bucket_count ; ++i) {
            concurrent_maps_[i];
        }
    };

    Access operator[](const Key& key) {
        auto partToLock = key % buck_C;
        return {std::lock_guard<std::mutex>(m_[partToLock]), concurrent_maps_[partToLock][key]};
    };

    void Erase(const Key& key) {
        auto partToLock = key % buck_C;
        std::lock_guard<std::mutex> lock(m_[partToLock]);
        concurrent_maps_[partToLock].erase(key);
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> ordinaryMap;
        for (size_t i = 0 ; i < buck_C ; ++i) {
            std::lock_guard<std::mutex> a (m_[i]);
            ordinaryMap.insert(concurrent_maps_[i].begin(), concurrent_maps_[i].end());
        }
        return ordinaryMap;
    };

private:
    uint64_t buck_C;
    std::vector<std::mutex> m_;
    std::map<Key, std::map<Key, Value>> concurrent_maps_;
};