//
// Created by evgenij on 11.10.2020.
//

#include "StripedLRU.h"

namespace Afina {
namespace Backend {

bool StripedLRU::Put(const std::string &key, const std::string &value)  {
    return stripe_regions[hash_stripes(key) % stripe_count]->Put(key, value)
}

// Implements Afina::Storage interface
bool StripedLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    return stripe_regions[hash_stripes(key) % stripe_count]->PutIfAbsent(key, value)
}

// Implements Afina::Storage interface
bool StripedLRU::Set(const std::string &key, const std::string &value) {
    return stripe_regions[hash_stripes(key) % stripe_count]->Set(key, value)
}

// Implements Afina::Storage interface
bool StripedLRU::Delete(const std::string &key)  {
    return stripe_regions[hash_stripes(key) % stripe_count]->Delete(key);
}

// Implements Afina::Storage interface
bool StripedLRU::Get(const std::string &key, std::string &value)  {
    return stripe_regions[hash_stripes(key) % stripe_count]->Get(key, value);
}

}
}
