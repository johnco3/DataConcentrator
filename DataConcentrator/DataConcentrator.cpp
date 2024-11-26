#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <optional>
#include <compare>
#include <chrono>
#include <atomic>
#include <condition_variable>

struct DataSample {
    int value;
    double reliability;

    DataSample(int v, double r)
        : value(v)
        , reliability(r)
    {}

    // Spaceship operator for three-way comparison - Higher reliability first
    auto operator<=>(const DataSample& other) const {
        return reliability <=> other.reliability; 
    }
};

// Template-based thread-safe queue
template <typename T>
class ThreadSafeQueue {
private:
    std::priority_queue<T> queue; // Uses std::less<T> on a std::vector<T> by default
    mutable std::mutex mtx;
    std::condition_variable cv;

public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(item);
        cv.notify_one();
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mtx);
        if (queue.empty()) {
            return std::nullopt;
        }
        T item = queue.top();
        queue.pop();
        return item;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }
};

// Producer function to generate data samples
void produceData(ThreadSafeQueue<DataSample>& queue, int threadId) {
    for (int i = 0; i < 10; ++i) {
        double reliability = (threadId + 1) * (i + 1) / 10.0; // Example reliability calculation
        queue.push(DataSample(i + 1 + threadId * 10, reliability));
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate work
    }
}

// Data concentrator function to process the highest reliability data samples
void dataConcentrator(ThreadSafeQueue<DataSample>& queue1, ThreadSafeQueue<DataSample>& queue2, std::atomic<bool>& running) {
    while (running) {
        std::optional<DataSample> sample1 = queue1.pop();
        std::optional<DataSample> sample2 = queue2.pop();

        if (sample1 && sample2) {
            if (sample1->reliability >= sample2->reliability) {
                std::cout << "Processing sample from Queue1: value=" << sample1->value << ", reliability=" << sample1->reliability << std::endl;
            } else {
                std::cout << "Processing sample from Queue2: value=" << sample2->value << ", reliability=" << sample2->reliability << std::endl;
            }
        } else if (sample1) {
            std::cout << "Processing sample from Queue1: value=" << sample1->value << ", reliability=" << sample1->reliability << std::endl;
        } else if (sample2) {
            std::cout << "Processing sample from Queue2: value=" << sample2->value << ", reliability=" << sample2->reliability << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate processing delay
    }
    std::cout << "Data concentrator gracefully stopped." << std::endl;
}

// Main function
int main() {
    ThreadSafeQueue<DataSample> queue1;
    ThreadSafeQueue<DataSample> queue2;

    std::atomic<bool> running(true);

    std::thread producer1(produceData, std::ref(queue1), 1);
    std::thread producer2(produceData, std::ref(queue2), 2);

    std::thread concentrator(dataConcentrator, std::ref(queue1), std::ref(queue2), std::ref(running));

    producer1.join();
    producer2.join();

    // Allow the concentrator to process remaining samples
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Signal the concentrator to stop
    running = false;

    concentrator.join();

    return 0;
}
