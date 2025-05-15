#ifndef SIGMAPROTOCOL_H
#define SIGMAPROTOCOL_H
#pragma once

#include <SigmaLoger.h>
#include <map>
#include <list>
#include "SigmaProtocolDefs.h"
#include <esp_event.h>

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
        pkg.eventId = 0;
        pkg.isReSubscribe = true;
        Subscribe(pkg);
    };

    void Sent(String topic, String payload);
    virtual void Unsubscribe(String topic) = 0;
    virtual void Unsubscribe(TopicSubscription topic) { Unsubscribe(topic.topic); };

    virtual void Disconnect() = 0;
    virtual void Close() {  shouldConnect = false; Disconnect(); };
    virtual void Connect() = 0;
    virtual bool IsReady() = 0;
    virtual String GetName()=0;
    virtual void SetName(String name)=0;
    virtual void SetShouldConnect(bool shouldConnect) { this->shouldConnect = shouldConnect; };
    virtual bool GetShouldConnect() {return shouldConnect;};
    virtual void SetLoop(const esp_event_loop_handle_t eventLoop) { this->eventLoop = eventLoop; };
    virtual bool IsNetworkRequired() { return isNetworkRequired; };
    virtual bool BeginSetup() = 0;
    virtual bool FinalizeSetup() = 0;
protected:
    SigmaLoger *PLogger = new SigmaLoger(512);
    bool isNetworkRequired = false;
    bool shouldConnect = true;
    String rootTopic = "/";
    inline static std::list<Message> messages;
    virtual void Publish(String topic, String payload) = 0;
    static esp_err_t PostEvent(int32_t event_id, void *event_data,size_t size);
    inline static SemaphoreHandle_t queueMutex = nullptr;
    inline static esp_event_loop_handle_t eventLoop=nullptr;
   
private:
    

};

#endif