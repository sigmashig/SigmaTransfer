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
    virtual String GetName() = 0;
    //virtual void SetShouldConnect(bool shouldConnect) = 0;
    //virtual bool GetShouldConnect() = 0;
    virtual bool IsNetworkRequired() { return isNetworkRequired; };
    //virtual bool BeginSetup() = 0;
    //virtual bool FinalizeSetup() = 0;
    static bool IsIP(String URL);
    static esp_event_loop_handle_t GetEventLoop() { return eventLoop; };
 
protected:

    SigmaLoger *PLogger = new SigmaLoger(512);
    bool isNetworkRequired = false;
    String rootTopic = "/";
    //inline static std::list<Message> messages;
    //virtual void send(String topic, String payload) = 0;
    bool isReady = false;
    virtual void setReady(bool ready) = 0;
    // virtual void SendBinary(byte *data, size_t size) = 0;
    void setRootTopic(String rootTopic) { this->rootTopic = rootTopic; };
    // esp_err_t PostEvent(int32_t eventId, void *eventData = nullptr, size_t eventDataSize = 0);
    //inline static SemaphoreHandle_t queueMutex = nullptr;
    //   inline static esp_event_loop_handle_t eventLoop = nullptr;
    std::map<String, TopicSubscription> subscriptions;
    TopicSubscription *GetSubscription(String topic);
private:
    const esp_event_loop_args_t loop_args = {
        .queue_size = 100,
        .task_name = "ProtocolEventLoop",
        .task_priority = 5, //priority must be lower the network loop
        .task_stack_size = 4096,
        .task_core_id = 1};
    static esp_event_loop_handle_t eventLoop;
};

#endif