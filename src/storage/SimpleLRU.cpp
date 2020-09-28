#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

    void SimpleLRU::delete_chosen_node(SimpleLRU::lru_node &node_to_del) {
        if (node_to_del.prev == nullptr && node_to_del.next == nullptr) { // case head == tail
            _lru_tail = nullptr;
            _lru_head = nullptr;
        } else if (node_to_del.prev == nullptr) { //case deleting head
            _lru_head = std::move(_lru_head->next);
            _lru_head->prev = nullptr;
        } else if (node_to_del.next == nullptr) { //case deleting tail
            lru_node* new_tail = _lru_tail->prev;
            new_tail->next = nullptr;
            _lru_tail = new_tail;
        } else {
            lru_node* prev = node_to_del.prev;
            prev->next = std::move(node_to_del.next);
            prev->next->prev = prev;
        }
    }

    void SimpleLRU::delete_lru_node() {
        _lru_index.erase(_lru_head->key);
        _cur_size -= _lru_head->key.size() + _lru_head->value.size();

        if (_lru_head->next != nullptr) {
            _lru_head = std::move(_lru_head->next);
            _lru_head->prev = nullptr;
        } else {
            _lru_head = nullptr;
            _lru_tail = nullptr;
        }
    }


    void SimpleLRU::move_node_to_tail(SimpleLRU::lru_node &node_found) {
        if (node_found.next != nullptr) {// moving recently used to the tail
            auto ptr = node_found.next->prev;
            ptr->next->prev = ptr->prev;
            if (ptr->prev != nullptr) {
                _lru_tail->next = std::move(ptr->prev->next);
                ptr->prev->next = std::move(ptr->next);
            } else { //case moving head
                _lru_tail->next = std::move(_lru_head);
                _lru_head = std::move(ptr->next);
            }
            ptr->next = nullptr;
            ptr->prev = _lru_tail;
            _lru_tail = ptr;
        }
    }

    bool SimpleLRU::_set_node_new_value(lru_node& node_found, const std::string &value) {
        if (node_found.key.size() + value.size() > _max_size) { //checking size
            return false;
        }

        move_node_to_tail(node_found);


        while (_cur_size + value.size() - node_found.value.size() > _max_size) { //deleting lru
            delete_lru_node();
        }
        _cur_size = _cur_size + value.size() - node_found.value.size();
        node_found.value = value;

        return true;
    }

    bool SimpleLRU::_put_new_node_with_value(const std::string &key, const std::string &value) {
        size_t ovr_size = key.size() + value.size();

        //case storage is empty
        if (_lru_head == nullptr and _lru_tail == nullptr) {
            if (ovr_size <= _max_size) {
                auto new_node = new lru_node{key, value, nullptr, nullptr};
                auto ptr = std::unique_ptr<lru_node>(new_node);
                _lru_head = std::move(ptr);
                _lru_tail = _lru_head.get();
                _lru_index.insert({std::cref(_lru_head->key), std::ref(*new_node)});
                _cur_size += ovr_size;
                return true;
            } else {
                return false;
            }
        }

        if (ovr_size <= _max_size) { // case we can insert
            while (_cur_size + ovr_size > _max_size) { // delete lru while there is not enough space
                delete_lru_node();
            }
            auto new_node = new lru_node{key, value, nullptr, nullptr}; //inserting
            auto ptr = std::unique_ptr<lru_node>(new_node);
            ptr->prev = _lru_tail;
            _lru_tail->next = std::move(ptr);
            _lru_tail = _lru_tail->next.get();

            _lru_index.insert({std::cref(_lru_tail->key), std::ref(*new_node)});

            _cur_size += ovr_size;
            return true;
        } else { // case inserting is impossible
            return false;
        }

    }

    // See MapBasedGlobalLockImpl.h
    bool SimpleLRU::Put(const std::string &key, const std::string &value) {
        auto search = _lru_index.find(key);
        if (search == _lru_index.end()) {
            return _put_new_node_with_value(key, value);
        } else {
            lru_node& node_found = search->second.get();
            return _set_node_new_value(node_found, value);
        }
    }

    // See MapBasedGlobalLockImpl.h
    bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
        auto search = _lru_index.find(key);
        if (search == _lru_index.end()) {
            return _put_new_node_with_value(key, value);
        } else {
            return false;
        }
    }


    // See MapBasedGlobalLockImpl.h
    bool SimpleLRU::Set(const std::string &key, const std::string &value) {
        auto search = _lru_index.find(key);
        if (search == _lru_index.end()) { //key not found
            return false;
        }
        lru_node& node_found = search->second.get();
        return _set_node_new_value(node_found, value);
    }

    // See MapBasedGlobalLockImpl.h
    bool SimpleLRU::Delete(const std::string &key) {
        auto search = _lru_index.find(key);
        if (search == _lru_index.end()) {
            return false;
        }

        lru_node& node_to_del = search->second.get();
        _lru_index.erase(key);
        _cur_size -= key.size() + node_to_del.value.size();
        delete_chosen_node(node_to_del);
        return true;
    }


    // See MapBasedGlobalLockImpl.h
    bool SimpleLRU::Get(const std::string &key, std::string &value) {
        auto search = _lru_index.find(key);
        if (search == _lru_index.end()) {
            return false;
        }
        lru_node& node_found = search->second.get();
        value = node_found.value;

        move_node_to_tail(node_found);

        return true;

    }

} // namespace Backend
} // namespace Afina