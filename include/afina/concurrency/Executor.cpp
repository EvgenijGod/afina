#include "Executor.h"

namespace Afina {
    namespace Concurrency {
        void Executor::Start() {
            std::unique_lock<std::mutex> lock(this->mutex);
            if (state == State::kRun) {
                return;
            }
            while (state != State::kStopped) {
                stop_condition.wait(lock);
            }
            for (size_t i = 0; i < low_watermark; i++) {
                std::thread new_thread(&Executor::perform, this);
                new_thread.detach();
            }
            threads_cnt = low_watermark;
            free_threads = low_watermark;
            state = State::kRun;
        }

        void Executor::Stop(bool await) {
            {
                std::unique_lock<std::mutex> lock(this->mutex);
                if (state == State::kStopped) {
                    return;
                }

                if (state == State::kRun) {
                    state = State::kStopping;
                    if (threads_cnt != 0) {
                        empty_condition.notify_all();
                    } else {
                        state = State::kStopped;
                    }
                }

                if (await) {
                    while (state != State::kStopped) {
                        stop_condition.wait(lock);
                    }
                }
            }
        }



        void Executor::perform() {
            std::unique_lock<std::mutex> lock(mutex);
            auto now = std::chrono::system_clock::now();
            while (true) {
                if (tasks.empty()) {
                    if (state == State::kStopping) {
                        // stopping thread after while
                        break;
                    } else {
                        if (empty_condition.wait_until(lock, now + idle_time) == std::cv_status::timeout &&
                            threads_cnt > low_watermark) {
                            // too much free threads
                            // stopping thread after while
                            break;
                        } else {
                            // timeout in low_watermark threads or thread was notified
                            // (we'll check why it was notified at the next iteration of while
                            continue;
                        }
                    }
                } else {
                    // there is a task
                    auto task = tasks.front();
                    tasks.pop_front();
                    free_threads--;
                    lock.unlock();
                    try {
                        task();
                    } catch (const std::exception &ex) {
                        std::cerr << "Error in executing function :" << ex.what() << std::endl;
                    }
                    lock.lock();
                    free_threads++;
                }
            }
            // stopping thread
            if (--threads_cnt == 0 && state == State::kStopping) {
                state = State::kStopped;
                stop_condition.notify_all();
            }
            free_threads--;
        }

    }
}