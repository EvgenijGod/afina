//
// Created by evgenij on 11.10.2020.
//

#include "StripedLRU.h"

namespace Afina {
namespace Backend {

bool StripedLRU::Put(const std::string &key, const std::string &value)  {
    return stripe_regions[hash_stripes(key) % stripe_count]->Put(key, value);
}

// Implements Afina::Storage interface
bool StripedLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    return stripe_regions[hash_stripes(key) % stripe_count]->PutIfAbsent(key, value);
}

// Implements Afina::Storage interface
bool StripedLRU::Set(const std::string &key, const std::string &value) {
    return stripe_regions[hash_stripes(key) % stripe_count]->Set(key, value);
}

// Implements Afina::Storage interface
bool StripedLRU::Delete(const std::string &key)  {
    return stripe_regions[hash_stripes(key) % stripe_count]->Delete(key);
}

// Implements Afina::Storage interface
bool StripedLRU::Get(const std::string &key, std::string &value)  {
    return stripe_regions[hash_stripes(key) % stripe_count]->Get(key, value);
}

StripedLRU* BuildStripedLRU(std::size_t memory_limit, std::size_t stripe_count) {
    std::size_t stripe_limit = memory_limit / stripe_count;
    if (stripe_limit < 1024 * 1024) {
        throw std::runtime_error("sufficient storage size, min 1 mb");
    }
    return new StripedLRU(stripe_count, memory_limit);
}
}
}
