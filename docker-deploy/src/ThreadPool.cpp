#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <algorithm>

class ThreadPool {
public:
    ThreadPool(size_t min_threads, size_t max_threads)
        : min_threads_(min_threads),
          max_threads_(max_threads),
          stop_(false),
          idle_threads_(0) {
        for (size_t i = 0; i < min_threads_; ++i) {
            workers_.emplace_back([this] { worker(); });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.push(task);
            
            // 动态增加线程：无空闲线程且未达上限
            if (idle_threads_ == 0 && workers_.size() < max_threads_) {
                workers_.emplace_back([this] { worker(); });
            }
        }
        condition_.notify_one();
    }

    // 新增状态获取接口
    struct ThreadStatus {
        size_t total_threads;   // 总线程数
        size_t idle_threads;    // 空闲线程数
        size_t pending_tasks;   // 等待任务数
    };

    ThreadStatus get_status(){
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return {
            workers_.size(),
            idle_threads_.load(),
            tasks_.size()
        };
    }

private:
    void worker() {
        while (!stop_) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                idle_threads_++;
                
                // 带超时的等待，用于动态减少线程
                bool timeout = !condition_.wait_for(
                    lock, 
                    std::chrono::seconds(1),
                    [this] { return stop_ || !tasks_.empty(); }
                );
                
                idle_threads_--;

                if (stop_ && tasks_.empty()) return;

                if (timeout) {
                    // 超时且线程数超过最小值，自动退出
                    if (workers_.size() > min_threads_) {
                        auto it = std::find_if(
                            workers_.begin(), workers_.end(),
                            [id = std::this_thread::get_id()](std::thread& t) {
                                return t.get_id() == id;
                            }
                        );
                        if (it != workers_.end()) {
                            it->detach();
                            workers_.erase(it);
                        }
                        return;
                    }
                    continue;
                }

                if (!tasks_.empty()) {
                    task = tasks_.front();
                    tasks_.pop();
                }
            }
            if (task) task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    
    std::atomic<bool> stop_;
    std::atomic<size_t> idle_threads_;
    
    const size_t min_threads_;
    const size_t max_threads_;
};