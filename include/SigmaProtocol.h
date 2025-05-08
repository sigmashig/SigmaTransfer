#ifndef SIGMAPROTOCOL_H
#define SIGMAPROTOCOL_H
#pragma once

#include <SigmaLoger.h>
#include <map>
#include "SigmaProtocolDefs.h"
class SigmaProtocol
{
public:
    SigmaProtocol();
    ~SigmaProtocol();
    virtual void Subscribe(TopicSubscription subscriptionTopic, String rootTopic = "") = 0;
    virtual void Subscribe(String topic)
    {
        TopicSubscription pkg;
        pkg.topic = topic;
        pkg.eventId = 0;
        pkg.isReSubscribe = true;
        Subscribe(pkg);
    };

    virtual void Publish(String topic, String payload) = 0;
    virtual void Unsubscribe(String topic, String rootTopic = "") = 0;
    virtual void Unsubscribe(TopicSubscription topic, String rootTopic = "") { Unsubscribe(topic.topic, rootTopic); };

    virtual void Disconnect() = 0;
    virtual void Connect() = 0;
    virtual bool IsConnected() = 0;
    virtual bool IsWiFiRequired() { return isWiFiRequired; };
    virtual bool Begin() = 0;
   
protected:
    bool isWiFiRequired = false;
private:
    

};

#endif