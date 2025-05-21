#ifndef SIGMAPROTOCOL_H
#define SIGMAPROTOCOL_H
#pragma once

#include <SigmaLoger.h>
#include <map>
#include <list>
#include <esp_event.h>
#include <SigmaAsyncNetwork.h>
// ESP_EVENT_DEFINE_BASE(SIGMAPROTOCOL_EVENT);
typedef struct
{
    String topic;
    int32_t eventId = PROTOCOL_SEND_SIGMA_MESSAGE;
    bool isReSubscribe = true;
} TopicSubscription;

typedef struct
{
    String protocolName;
    String topic;
    String payload;
} Message;

typedef struct
{
    byte *data;
    size_t size;
} BinaryData;

class SigmaProtocol
{
public:
    SigmaProtocol();
    ~SigmaProtocol();
    virtual void Subscribe(TopicSubscription subscriptionTopic) = 0;
    virtual void Subscribe(String topic)
    {
        TopicSubscription pkg;
        pkg.topic = rootTopic + topic;
        Subscribe(pkg);
    };
    virtual void Unsubscribe(String topic) = 0;
    virtual void Unsubscribe(TopicSubscription topic) { Unsubscribe(topic.topic); };

    virtual void Disconnect() = 0;
    virtual void Close() = 0;
    virtual void Connect() = 0;
    virtual bool IsReady() { return isReady; };
    virtual bool IsNetworkRequired() { return isNetworkRequired; };
    static bool IsIP(String URL);
    virtual void SetClientId(String id) { clientId = id; };
    static esp_event_loop_handle_t GetEventLoop() { return eventLoop; };
    virtual String GetClientId() { return clientId; };
    virtual String GetName() { return name; };

protected:
    SigmaLoger *PLogger = new SigmaLoger(512);
    bool isNetworkRequired = false;
    String rootTopic = "/";
    bool isReady = false;
    virtual void setReady(bool ready) = 0;
    void setRootTopic(String rootTopic) { this->rootTopic = rootTopic; };
    std::map<String, TopicSubscription> subscriptions;
    TopicSubscription *GetSubscription(String topic);
    void addSubscription(TopicSubscription subscription);
    void removeSubscription(String topic);
    String clientId;
    String name;

private:
    const esp_event_loop_args_t loop_args = {
        .queue_size = 100,
        .task_name = "ProtocolEventLoop",
        .task_priority = 5, // priority must be lower the network loop
        .task_stack_size = 4096,
        .task_core_id = 1};
    inline static esp_event_loop_handle_t eventLoop;
};

#endif