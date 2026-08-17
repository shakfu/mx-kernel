#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace mx {

// A single typed atom value for outlet messages (mirrors Max atom types).
using AtomValue = std::variant<std::string, long, double>;

// Message destined for a Max outlet (produced by kernel thread, consumed on main thread).
struct OutletMessage {
    std::string selector;
    std::vector<AtomValue> atoms;
    int outlet_index = 0; // 0 = left, 1 = right
    int execution_counter = 0;
};

// Result flowing back from Max to the kernel thread.
//
// execution_counter is stamped by the external at push time with the cell that
// was waiting when the message arrived (0 means "no cell was waiting"). The
// interpreter discards any result whose counter does not match its own, which
// keeps a late or duplicated reply from being attributed to the wrong cell.
struct ResultMessage {
    std::string text;
    std::string error_name;  // non-empty => error
    std::string error_value;
    std::string stream_name; // "stdout" / "stderr", empty => execution_result
    std::string mime_type;   // e.g. "application/json", empty => text/plain
    int execution_counter = 0;

    bool is_error() const { return !error_name.empty(); }

    // A stream message is intermediate output: it does not complete a cell.
    bool is_stream() const { return !stream_name.empty(); }
};

// Minimal thread-safe FIFO queue. Mutex + deque + condition variable, so a
// consumer can block until an item arrives instead of polling.
template <typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push_back(std::move(item));
        }
        m_cv.notify_one();
    }

    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return std::nullopt;
        }
        T item = std::move(m_queue.front());
        m_queue.pop_front();
        return item;
    }

    // Block until an item is available or the timeout elapses. Returns
    // std::nullopt on timeout. A zero or negative timeout degrades to try_pop.
    std::optional<T> wait_pop(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (timeout.count() > 0) {
            m_cv.wait_for(lock, timeout, [this] { return !m_queue.empty(); });
        }
        if (m_queue.empty()) {
            return std::nullopt;
        }
        T item = std::move(m_queue.front());
        m_queue.pop_front();
        return item;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.clear();
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<T> m_queue;
};

} // namespace mx
