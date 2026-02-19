#pragma once

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
struct ResultMessage {
    std::string text;
    std::string error_name;  // non-empty => error
    std::string error_value;
    std::string stream_name; // "stdout" / "stderr", empty => execution_result
    std::string mime_type;   // e.g. "application/json", empty => text/plain
    int execution_counter = 0;

    bool is_error() const { return !error_name.empty(); }
};

// Minimal thread-safe FIFO queue. Mutex + deque -- correct and simple for
// human-typing-speed message rates.
template <typename T>
class ThreadSafeQueue {
public:
    void push(T item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(std::move(item));
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
    std::deque<T> m_queue;
};

} // namespace mx
