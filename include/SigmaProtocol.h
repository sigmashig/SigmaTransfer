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
enum AuthType
{
    AUTH_TYPE_NONE = 0,
    AUTH_TYPE_URL = 0x01,
    AUTH_TYPE_BASIC = 0x02,
    AUTH_TYPE_FIRST_MESSAGE = 0x04,
    AUTH_TYPE_ALL_MESSAGES = 0x08 // All messages are authenticated. No Binary Data. Text/Sigma Only. Converted to Json
};

class SigmaProtocol
{
public:
    SigmaProtocol(String name, uint priority = 5, uint queueSize = 100, uint stackSize = 4096, uint coreId = 1);
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
    virtual esp_event_loop_handle_t GetEventLoop() { return eventLoop; };
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
    //esp_event_loop_handle_t* createEventLoop(esp_event_loop_handle_t* eventLoop, String name, uint priority=5, uint queueSize=100, uint stackSize=4096, uint coreId=1);
    esp_event_loop_handle_t eventLoop;

private : 
};

#endif