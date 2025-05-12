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
    String topic;
    String username="";
    String password="";
    String clientId="Client_"+String(ESP.getEfuseMac(),HEX);
} MqttConfig;



class SigmaMQTT : public SigmaProtocol
{
public:
    //[[deprecated("Use a single parameter url instead")]] static void Init(IPAddress ip, String url = "", uint16_t port = 1883, String user = "", String pwd = "");
    //SigmaMQTT(String url, uint16_t port = 1883, String user = "", String pwd = "", String clientId = "");
    SigmaMQTT(MqttConfig config);
    SigmaMQTT();

    //void Init(String url, uint16_t port = 1883, String user = "", String pwd = "", String clientId = "");
    void SetParams(MqttConfig config);
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

    void SetClientId(String id) { clientId= id; };
    void Connect();
    void Disconnect();
    bool IsConnected();
    bool IsWiFiRequired() { return true; };
    String GetName() { return name; };
    void SetName(String name) { Serial.printf("SetName=%s\n",name.c_str()); this->name = name; };
private:

    MqttConfig config;
    inline static SigmaLoger *MLogger = new SigmaLoger(512);
    static void ConnectToMqtt();

    inline static String name;
    //inline static TimerHandle_t mqttReconnectTimer;
    inline static String clientId;
    inline static AsyncMqttClient mqttClient;
    static void onMqttConnect(bool sessionPresent);
    static void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    static void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);

    inline static std::map<String, TopicSubscription> eventMap;
    inline static std::map<String, String> topicMsg;
};

#endif