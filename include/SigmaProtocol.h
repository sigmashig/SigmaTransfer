#ifndef SIGMAPROTOCOL_H
#define SIGMAPROTOCOL_H
#pragma once

#include <SigmaLoger.h>
#include <map>
#include <list>
#include <esp_event.h>
//#include <SigmaAsyncNetwork.h>
#include "SigmaTransferDefs.h"

typedef struct
{
    String protocolName;
    String topic;
    String payload;
} Message;

class SigmaProtocol
{
public:
    SigmaProtocol(String name, SigmaLoger *logger = nullptr, uint priority = 5, uint queueSize = 100, uint stackSize = 4096, uint coreId = 1);
    ~SigmaProtocol();
    virtual void Subscribe(TopicSubscription subscriptionTopic) {
        addSubscription(subscriptionTopic);
    };

    virtual void Subscribe(String topic)
    {
        TopicSubscription pkg;
        pkg.topic = rootTopic + topic;
        Subscribe(pkg);
    };
    virtual void Unsubscribe(String topic) {
        removeSubscription(topic);
    };
    virtual void Unsubscribe(TopicSubscription topic) { Unsubscribe(topic.topic); };

    //virtual bool IsNetworkRequired() { return isNetworkRequired; };
    static bool IsIP(String URL);
    virtual esp_event_loop_handle_t GetEventLoop() { return eventLoop; };
    virtual esp_event_base_t GetEventBase() { return eventBase; };
    virtual String GetName() { return name; };
    virtual bool IsReady() { return isReady; };

protected:
    
    esp_event_base_t eventBase = "SigmaProtocol";
    //bool isNetworkRequired = false;
    String rootTopic = "/";
    bool isReady = false;
    std::map<String, TopicSubscription> subscriptions;
    String clientId;
    String name;
    esp_event_loop_handle_t eventLoop;
    SigmaLoger *Log;

    virtual void Disconnect() = 0;
    virtual void Close() = 0;
    virtual void Connect() = 0;
    virtual void setReady(bool ready){isReady = ready;};
    void setRootTopic(String rootTopic) { this->rootTopic = rootTopic; };
    TopicSubscription *GetSubscription(String topic);
    void addSubscription(TopicSubscription subscription);
    void removeSubscription(String topic);

private : 
};

#endif