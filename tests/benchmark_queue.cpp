// benchmark_queue.cpp - Lock-free queue performance benchmark
// OBJECTIVE: Validate < 1μs enqueue/dequeue latency (Gate 3b requirement)

#include "MarketData.h"
#include <concurrentqueue.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <thread>

using namespace std::chrono;

// PERFORMANCE: High-resolution timestamp measurement
inline auto now() {
    return high_resolution_clock::now();
}

// PERFORMANCE: Calculate latency statistics
struct Stats {
    double min_us;
    double max_us;
    double mean_us;
    double p50_us;
    double p95_us;
    double p99_us;
    
    void calculate(std::vector<double>& latencies) {
        if (latencies.empty()) return;
        
        std::sort(latencies.begin(), latencies.end());
        
        min_us = latencies.front();
        max_us = latencies.back();
        
        double sum = 0;
        for (auto l : latencies) sum += l;
        mean_us = sum / latencies.size();
        
        p50_us = latencies[latencies.size() * 50 / 100];
        p95_us = latencies[latencies.size() * 95 / 100];
        p99_us = latencies[latencies.size() * 99 / 100];
    }
    
    void print(const std::string& label) const {
        std::cout << "\n" << label << " Latency Statistics:\n";
        std::cout << "  Min:  " << std::fixed << std::setprecision(3) << min_us << " μs\n";
        std::cout << "  Mean: " << mean_us << " μs\n";
        std::cout << "  P50:  " << p50_us << " μs\n";
        std::cout << "  P95:  " << p95_us << " μs\n";
        std::cout << "  P99:  " << p99_us << " μs\n";
        std::cout << "  Max:  " << max_us << " μs\n";
    }
};

// REASON: Single-threaded enqueue benchmark (producer-only)
void benchmarkEnqueue(int iterations) {
    std::cout << "\n=== Single-Threaded Enqueue Benchmark ===\n";
    std::cout << "Iterations: " << iterations << "\n";
    
    moodycamel::ConcurrentQueue<TickUpdate> queue(iterations);
    std::vector<double> latencies;
    latencies.reserve(iterations);
    
    // Warm-up: 1000 iterations
    TickUpdate warmup;
    for (int i = 0; i < 1000; ++i) {
        queue.try_enqueue(warmup);
    }
    
    // Benchmark
    for (int i = 0; i < iterations; ++i) {
        TickUpdate update;
        update.tickerId = i;
        update.type = TickUpdateType::BidAsk;
        update.timestamp = i * 1000;
        update.bidPrice = 100.0 + i * 0.01;
        update.askPrice = 100.05 + i * 0.01;
        update.bidSize = 100;
        update.askSize = 100;
        
        auto start = now();
        queue.try_enqueue(update);
        auto end = now();
        
        double latency_us = duration_cast<nanoseconds>(end - start).count() / 1000.0;
        latencies.push_back(latency_us);
    }
    
    Stats stats;
    stats.calculate(latencies);
    stats.print("Enqueue");
    
    // Gate 3b validation
    if (stats.p50_us < 1.0) {
        std::cout << "\n✅ GATE 3b PASSED: Enqueue latency < 1μs (p50 = " 
                  << stats.p50_us << " μs)\n";
    } else {
        std::cout << "\n❌ GATE 3b FAILED: Enqueue latency >= 1μs (p50 = " 
                  << stats.p50_us << " μs)\n";
    }
}

// REASON: Single-threaded dequeue benchmark (consumer-only)
void benchmarkDequeue(int iterations) {
    std::cout << "\n=== Single-Threaded Dequeue Benchmark ===\n";
    std::cout << "Iterations: " << iterations << "\n";
    
    moodycamel::ConcurrentQueue<TickUpdate> queue(iterations);
    std::vector<double> latencies;
    latencies.reserve(iterations);
    
    // Pre-populate queue
    for (int i = 0; i < iterations; ++i) {
        TickUpdate update;
        update.tickerId = i;
        update.type = TickUpdateType::BidAsk;
        update.timestamp = i * 1000;
        update.bidPrice = 100.0 + i * 0.01;
        update.askPrice = 100.05 + i * 0.01;
        queue.try_enqueue(update);
    }
    
    // Benchmark
    TickUpdate update;
    for (int i = 0; i < iterations; ++i) {
        auto start = now();
        queue.try_dequeue(update);
        auto end = now();
        
        double latency_us = duration_cast<nanoseconds>(end - start).count() / 1000.0;
        latencies.push_back(latency_us);
    }
    
    Stats stats;
    stats.calculate(latencies);
    stats.print("Dequeue");
    
    // Gate 3b validation
    if (stats.p50_us < 1.0) {
        std::cout << "\n✅ GATE 3b PASSED: Dequeue latency < 1μs (p50 = " 
                  << stats.p50_us << " μs)\n";
    } else {
        std::cout << "\n❌ GATE 3b FAILED: Dequeue latency >= 1μs (p50 = " 
                  << stats.p50_us << " μs)\n";
    }
}

// REASON: Producer-consumer benchmark (realistic workload)
void benchmarkProducerConsumer(int iterations) {
    std::cout << "\n=== Producer-Consumer Benchmark ===\n";
    std::cout << "Iterations: " << iterations << "\n";
    
    moodycamel::ConcurrentQueue<TickUpdate> queue(iterations);
    std::atomic<int> enqueued{0};
    std::atomic<int> dequeued{0};
    std::vector<double> latencies;
    
    // Use mutex for latencies vector to avoid concurrent modification
    std::mutex latencies_mutex;
    
    // Producer thread: enqueue with timestamps
    std::thread producer([&]() {
        for (int i = 0; i < iterations; ++i) {
            TickUpdate update;
            update.tickerId = i;
            update.type = TickUpdateType::BidAsk;
            update.timestamp = duration_cast<nanoseconds>(now().time_since_epoch()).count();
            update.bidPrice = 100.0 + i * 0.01;
            update.askPrice = 100.05 + i * 0.01;
            
            queue.enqueue(update);
            enqueued.fetch_add(1, std::memory_order_release);
        }
    });
    
    // Consumer thread: dequeue and measure end-to-end latency
    std::thread consumer([&]() {
        TickUpdate update;
        
        while (dequeued.load(std::memory_order_acquire) < iterations) {
            if (queue.try_dequeue(update)) {
                auto now_ns = duration_cast<nanoseconds>(now().time_since_epoch()).count();
                double latency_us = (now_ns - update.timestamp) / 1000.0;
                
                std::lock_guard<std::mutex> lock(latencies_mutex);
                latencies.push_back(latency_us);
                dequeued.fetch_add(1, std::memory_order_release);
            } else {
                // Brief sleep to avoid busy-wait
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    Stats stats;
    stats.calculate(latencies);
    stats.print("End-to-End (Producer → Consumer)");
}

int main(int argc, char* argv[]) {
    std::cout << "=== Lock-Free Queue Performance Benchmark ===\n";
    std::cout << "Target: < 1μs enqueue/dequeue latency (Gate 3b)\n";
    
    int iterations = 100000;  // 100K iterations for statistical significance
    
    if (argc > 1) {
        iterations = std::atoi(argv[1]);
    }
    
    benchmarkEnqueue(iterations);
    benchmarkDequeue(iterations);
    benchmarkProducerConsumer(iterations);
    
    std::cout << "\n=== Benchmark Complete ===\n";
    
    return 0;
}
