//
// Created by evgenij on 11.10.2020.
//

#ifndef AFINA_STRIPEDLRU_H
#define AFINA_STRIPEDLRU_H

#include <afina/Storage.h>
#include "ThreadSafeSimpleLRU.h"

#include <vector>

namespace Afina {
namespace Backend {

class StripedLRU : public Afina::Storage{
    std::hash<std::string> hash_stripes;
    std::size_t stripe_count;
    std::vector<std::unique_ptr<ThreadSafeSimplLRU>> stripe_regions;
public:
    ~StripedLRU() {}


    StripedLRU(std::size_t stripe_count = 1024, std::size_t memory_limit = 1024 * 1000)
        : stripe_count(stripe_count) {
        for (size_t i = 0; i < stripe_count; i++) {
            stripe_regions.emplace_back(new ThreadSafeSimplLRU(memory_limit));

        }
    }
    bool Put(const std::string &key, const std::string &value) override ;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override ;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override ;

};

StripedLRU* BuildStripedLRU(std::size_t memory_limit, std::size_t stripe_count);

}
}




#endif //AFINA_STRIPEDLRU_H
