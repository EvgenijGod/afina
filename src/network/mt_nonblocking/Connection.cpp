#include "Connection.h"

#include <cassert>
#include <exception>
#include <iostream>
#include <unistd.h>

namespace Afina {
namespace Network {
namespace MTnonblock {

// See Connection.h
void Connection::Start() {
    //std::lock_guard<std::mutex> lock(con_mutex);
    if (is_started) {
        throw std::runtime_error("calling Start at the same connection twice");
    }
    is_alive = true;
    is_started = true;
    read_begin = read_end = 0;
    shift = 0;
    _event.events = EPOLLIN|EPOLLERR|EPOLLRDHUP;
    _event.data.fd = _socket;
    _event.data.ptr = this;
}

// See Connection.h
void Connection::OnError() {
    //std::lock_guard<std::mutex> lock(con_mutex);
    _event.events = 0;
    is_alive.store(false, std::memory_order_relaxed);
}

// See Connection.h
void Connection::OnClose() {
    //std::lock_guard<std::mutex> lock(con_mutex);
    _event.events = 0;
    is_alive.store(false, std::memory_order_relaxed);
}

// See Connection.h
void Connection::Close() {
    //std::lock_guard<std::mutex> lock(con_mutex);
    is_alive.store(false, std::memory_order_relaxed);
}

// See Connection.h
void Connection::DoRead() {
    std::atomic_thread_fence(std::memory_order_acquire);

    try {
        if ((readed_bytes = read(_socket, read_buf + read_end, buf_size - read_end)) > 0) {
            read_end += readed_bytes;
            while (read_end - read_begin > 0) {
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(read_buf + read_begin, read_end - read_begin, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        read_begin += parsed;
                    }
                }
                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, read_end - read_begin);
                    argument_for_command.append(read_buf + read_begin, to_read);

                    arg_remains -= to_read;
                    read_begin += to_read;
                }
                // There are command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    std::string result;
                    if (argument_for_command.size()) {
                        argument_for_command.resize(argument_for_command.size() - 2);
                    }
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Put response in the queue
                    result += "\r\n";
                    responses.push_back(std::move(result));
                    if (!(_event.events & EPOLLOUT)) {
                        _event.events |= EPOLLOUT;
                    }

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            }
            if (read_begin == read_end) {
                read_begin = read_end = 0;
            } else if (read_end == buf_size) {
                std::memmove(read_buf, read_buf + read_begin, read_end - read_begin);
            }
        } else if (readed_bytes == -1){
            is_alive.store(false, std::memory_order_relaxed);
        }
        std::atomic_thread_fence(std::memory_order_release);
    } catch (std::runtime_error &ex) {
        responses.push_back("ERROR\r\n");
        if (!(_event.events & EPOLLOUT)) {
            _event.events |= EPOLLOUT;
            std::atomic_thread_fence(std::memory_order_release);
        }
    }

}

// See Connection.h
void Connection::DoWrite() {
    std::atomic_thread_fence(std::memory_order_acquire);

    //std::lock_guard<std::mutex> lock(con_mutex, std::adopt_lock);
    static constexpr size_t write_vec_size = 64;
    iovec write_vec[write_vec_size];
    size_t write_vec_v = 0;
    {
        auto it = responses.begin();
        assert(shift < it->size());
        write_vec[write_vec_v].iov_base = &((*it)[0]) + shift;
        write_vec[write_vec_v].iov_len = it->size() - shift;
        it++;
        write_vec_v++;
        for (; it != responses.end(); it++) {
            write_vec[write_vec_v].iov_base = &((*it)[0]);
            write_vec[write_vec_v].iov_len = it->size();
            if (++write_vec_v >= write_vec_size) {
                break;
            }
        }
    }

    int writed;
    if ((writed = writev(_socket, write_vec, write_vec_v)) > 0) {
        size_t i = 0;
        while (i < write_vec_v && writed >= write_vec[i].iov_len) {
            assert(responses.front().c_str() <= write_vec[i].iov_base &&
                   write_vec[i].iov_base < responses.front().c_str() + responses.front().size());
            responses.pop_front();
            writed -= write_vec[i].iov_len;
            i++;
        }
        shift = writed;
    }  else if (writed < 0 && writed != EAGAIN){
        is_alive.store(false, std::memory_order_relaxed);
    }

    if (responses.empty()) {
        _event.events &= ~EPOLLOUT;
    }
    if (responses.size() <= N){
        _event.events |= EPOLLIN;
    }

    std::atomic_thread_fence(std::memory_order_release);

}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina