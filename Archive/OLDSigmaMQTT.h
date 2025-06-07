#ifndef SIGMAMQTT_H
#define SIGMAMQTT_H

#pragma once
#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <SigmaLoger.h>
#include <SigmaInternalPkg.h>
#include <map>
#include <queue>
#include <esp_event.h>
#include "SigmaProtocol.h"

typedef struct
{
    String server;
    uint16_t port = 1883;
    String rootTopic;
    String username = "";
    String password = "";
    String clientId = "Client_" + String(ESP.getEfuseMac(), HEX);
    int keepAlive = 60;
} MqttConfig;
/*
typedef struct
{
    enum
    {
        Q_TIMER,
        Q_MQTTCONNECT,
        Q_MQTTDISCONNECT,
        Q_MQTTMESSAGE
    } type;
    TimerHandle_t timer;
    // AsyncMqttClient *client;
    bool sessionPresent;
    AsyncMqttClientMessageProperties properties;
    String topic;
    String payload;
    size_t len;
    size_t index;
    size_t total;
    AsyncMqttClientDisconnectReason reason;
} MqttQueueTask;
*/
class SigmaMQTT : public SigmaProtocol
{
public:
    SigmaMQTT(String name, SigmaLoger *logger, MqttConfig config, uint priority = 5);
    ~SigmaMQTT();

    void Subscribe(TopicSubscription subscriptionTopic);
    void Unsubscribe(String topic);
    bool IsReady();

private:
    MqttConfig config;
    void publish(SigmaInternalPkg *pkg);
    void Connect();
    void Disconnect();
    void Close()
    {
        shouldConnect = false;
        Disconnect();
    };
    // void send(String topic, String payload);

    TimerHandle_t mqttReconnectTimer;
    AsyncMqttClient mqttClient;
    SemaphoreHandle_t queueMutex;
    char clientid[64];
    char username[64];
    char password[64];

    static void onMqttConnect(bool sessionPresent);
    static void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    static void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
    static void reconnectByTimer(TimerHandle_t xTimer);
    static void processMqttQueue(void *arg);

    void onConnect(bool sessionPresent);
    void onDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMessage(const char *topic, const char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void onTimer(TimerHandle_t xTimer);

    std::map<String, String> topicMsg;
    std::queue<SigmaInternalPkg> messages;
    bool shouldConnect = true;
    // inline static QueueHandle_t mqttQueue;
    esp_event_loop_handle_t internalLoop;
    esp_event_base_t internalBase = "internalMQTT";
    static void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
};

#endif