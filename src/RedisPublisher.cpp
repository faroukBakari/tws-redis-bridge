// RedisPublisher.cpp - Redis Pub/Sub adapter implementation

#include "RedisPublisher.h"
#include <iostream>
#include <stdexcept>

namespace tws_bridge {

RedisPublisher::RedisPublisher(const std::string& uri)
    : m_uri(uri) {
    try {
        // REASON: redis-plus-plus automatically manages connection pool
        sw::redis::ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.socket_timeout = std::chrono::milliseconds(100);
        
        // REASON: Connection pool with 3 connections for thread-safety
        sw::redis::ConnectionPoolOptions pool_opts;
        pool_opts.size = 3;
        pool_opts.wait_timeout = std::chrono::milliseconds(100);
        
        m_redis = std::make_unique<sw::redis::Redis>(opts, pool_opts);
        
        // REASON: Test connection with PING
        m_redis->ping();
        
    } catch (const std::exception& e) {
        std::cerr << "[REDIS] Connection failed: " << e.what() << "\n";
        throw;
    }
}

RedisPublisher::~RedisPublisher() {
    std::cout << "[REDIS] Disconnecting...\n";
}

void RedisPublisher::publish(const std::string& channel, const std::string& message) {
    try {
        // PERFORMANCE: PUBLISH is O(N+M) where N=subscribers, M=channels
        // For single channel with few subscribers, this is ~100μs
        long long subscribers = m_redis->publish(channel, message);
        
        // PERFORMANCE: Reduce logging in production
        if (subscribers == 0) {
            // PITFALL: No subscribers is not an error (Redis doesn't buffer)
            // std::cout << "[REDIS] No subscribers for channel: " << channel << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "[REDIS] Publish error: " << e.what() << "\n";
        throw;
    }
}

bool RedisPublisher::isConnected() const {
    try {
        // REASON: PING is a lightweight health check
        m_redis->ping();
        return true;
    } catch (...) {
        return false;
    }
}

void RedisPublisher::reconnect() {
    std::cout << "[REDIS] Attempting reconnection...\n";
    
    try {
        sw::redis::ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.socket_timeout = std::chrono::milliseconds(100);
        
        sw::redis::ConnectionPoolOptions pool_opts;
        pool_opts.size = 3;
        pool_opts.wait_timeout = std::chrono::milliseconds(100);
        
        m_redis = std::make_unique<sw::redis::Redis>(opts, pool_opts);
        m_redis->ping();
        
        std::cout << "[REDIS] Reconnection successful\n";
    } catch (const std::exception& e) {
        std::cerr << "[REDIS] Reconnection failed: " << e.what() << "\n";
        throw;
    }
}

} // namespace tws_bridge
