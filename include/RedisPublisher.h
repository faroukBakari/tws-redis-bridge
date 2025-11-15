// RedisPublisher.h - Redis Pub/Sub abstraction (Adapter Pattern)
// SCOPE: Consumer thread - dequeues tick updates, serializes, publishes

#pragma once

#include <string>
#include <memory>
#include <sw/redis++/redis++.h>

namespace tws_bridge {

// REASON: Abstracts redis-plus-plus library, provides clean interface
class RedisPublisher {
public:
    explicit RedisPublisher(const std::string& uri);
    ~RedisPublisher();

    // REASON: Non-copyable (manages connection resource)
    RedisPublisher(const RedisPublisher&) = delete;
    RedisPublisher& operator=(const RedisPublisher&) = delete;

    // Publish JSON message to Redis channel
    // PERFORMANCE: Uses redis-plus-plus connection pool internally
    void publish(const std::string& channel, const std::string& message);
    
    // Connection health check
    bool isConnected() const;
    
    // Reconnect if connection lost
    void reconnect();

private:
    std::unique_ptr<sw::redis::Redis> m_redis;
    std::string m_uri;
};

} // namespace tws_bridge
