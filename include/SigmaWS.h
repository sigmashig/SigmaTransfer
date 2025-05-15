#ifndef SIGMAWS_H
#define SIGMAWS_H

#pragma once
#include <Arduino.h>
#include <SigmaLoger.h>
#include <SigmaInternalPkg.h>
#include <map>
#include <esp_event.h>
#include "SigmaProtocolDefs.h"
#include "SigmaProtocol.h"
#include <AsyncTCP.h>

typedef struct {
    String host;
    uint16_t port=80;
    String clientId;
    String rootTopic = "/";
    String apiKey = "";
    bool isPlain = false;
} WSConfig;


ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

class SigmaWS : public SigmaProtocol
{
public:
    SigmaWS(WSConfig config);
    SigmaWS();

    void SetParams(WSConfig config);
    bool BeginSetup();
    bool FinalizeSetup();
    void Subscribe(TopicSubscription subscriptionTopic, String rootTopic = "");
    void Subscribe(String topic)
    {
        TopicSubscription pkg;
        pkg.topic = topic;
        Subscribe(pkg);
    };
    void Publish(String topic, String payload);
    void Unsubscribe(String topic, String rootTopic = "");
    void Unsubscribe(TopicSubscription topic, String rootTopic = "") { Unsubscribe(topic.topic, rootTopic); };

    void Connect();
    void Disconnect();
    bool IsReady() { return isConnected; };
    bool IsNetRequired() { return true; };
    String GetName() { return name; };
    void SetName(String name) { Serial.printf("SetName=%s\n",name.c_str()); this->name = name; };
private:

    WSConfig config;
    bool isConnected = false;

    inline static String name;
    inline static String clientId;
    inline static std::map<String, TopicSubscription> eventMap;

    inline static AsyncClient *client;
};

#endif