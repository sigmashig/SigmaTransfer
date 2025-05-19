#ifndef SIGMAMQTT_H
#define SIGMAMQTT_H

#pragma once
#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <SigmaLoger.h>
#include <SigmaInternalPkg.h>
#include <map>
#include <esp_event.h>
#include "SigmaProtocolDefs.h"
#include "SigmaProtocol.h"

typedef struct {
    String server;
    uint16_t port=1883;
    String rootTopic;
    String username="";
    String password="";
    String clientId="Client_"+String(ESP.getEfuseMac(),HEX);
    int keepAlive=60;
} MqttConfig;



class SigmaMQTT : public SigmaProtocol
{
public:
    SigmaMQTT(MqttConfig config);
    SigmaMQTT();

    void SetParams(MqttConfig config);
    bool BeginSetup();
    bool FinalizeSetup();
    void Subscribe(TopicSubscription subscriptionTopic);
    void Publish(String topic, String payload);
    void Unsubscribe(String topic);

    void SetClientId(String id) { clientId= id; };
    void Connect();
    void Disconnect();
    bool IsReady();
    bool IsNetworkRequired() { return true; };
    String GetName() { return name; };
    void SetName(String name) { this->name = name; };
    void SetShouldConnect(bool shouldConnect) { this->shouldConnect = shouldConnect; };
    bool GetShouldConnect() { return shouldConnect; };
    void Close() { shouldConnect = false; Disconnect(); };
private:

    MqttConfig config;
    inline static SigmaLoger *MLogger = new SigmaLoger(512);

    inline static String name;
    inline static TimerHandle_t mqttReconnectTimer;
    inline static String clientId;
    inline static AsyncMqttClient mqttClient;
    static void onMqttConnect(bool sessionPresent);
    static void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    static void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);

    inline static std::map<String, TopicSubscription> eventMap;
    inline static std::map<String, String> topicMsg;
    inline static bool shouldConnect = true;
};

#endif