#ifndef SIGMAMQTT_H
#define SIGMAMQTT_H

#pragma once

#include "SigmaConnection.h"
#include "SigmaTransferDefs.h"
#include "SigmaInternalPkg.h"
#include <mqtt_client.h>

class SigmaMQTT : public SigmaConnection
{
public:
    SigmaMQTT(MqttConfig config, SigmaLoger *logger = nullptr, uint priority = 5);
    ~SigmaMQTT();
    void Subscribe(TopicSubscription subscriptionTopic);
    void Unsubscribe(String topic);
    // bool IsReady();
    void Connect();

private:
    MqttConfig config;
    void publish(SigmaInternalPkg *pkg);
    void Disconnect();
    void Close()
    {
        shouldConnect = false;
        Disconnect();
    };
    void sendPing() { /* nothing todo */ };
    void init();
    bool shouldConnect = false;
    esp_mqtt_client_handle_t mqttClient;
    static void onMqttEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    // static void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
};

#endif