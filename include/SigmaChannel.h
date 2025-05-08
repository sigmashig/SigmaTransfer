#ifndef SIGMACHANNEL_H
#define SIGMACHANNEL_H
#pragma once
#include <Arduino.h>
#include "SigmaProtocol.h"
#include "SigmaCrypt.h"
#include "SigmaProtocolDefs.h"

typedef struct
{
    String name = "";
    String correspondentId = "";
    String rootTopic = "/";
    SigmaProtocol *protocol;
    SigmaCrypt *crypt = NULL;
    int eventId = PROTOCOL_MESSAGE;
    bool isReSubscribe = true;
} SigmaChannelConfig;

class SigmaChannel
{
public:
    SigmaChannel(SigmaChannelConfig config);
    ~SigmaChannel();
    void Subscribe(TopicSubscription subscription) { config.protocol->Subscribe(subscription, config.rootTopic); }  ;
    void Send(String topic, String payload) { config.protocol->Publish(config.rootTopic + topic, payload); };
    void Unsubscribe(String topic) { config.protocol->Unsubscribe(config.rootTopic + topic); };
    void SetProtocol(SigmaProtocol *protocol) { config.protocol = protocol; };
    void SetCrypt(SigmaCrypt *crypt) { config.crypt = crypt; };
    void SetEventId(int eventId) { config.eventId = eventId; };
    void SetIsReSubscribe(bool isReSubscribe) { config.isReSubscribe = isReSubscribe; };
    SigmaChannelConfig GetConfig() { return config; };
private:
    SigmaChannelConfig config;
};

#endif