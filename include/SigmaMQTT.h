#ifndef SIGMAMQTT_H
#define SIGMAMQTT_H

#include "SigmaProtocol.h"
#include "SigmaInternalPkg.h"
#include "SigmaTransferDefs.h"
#include "mqtt_client.h"

#pragma once

class SigmaMQTT : public SigmaProtocol
{
public:
    SigmaMQTT(String name, SigmaLoger *logger, MqttConfig config, uint priority = 5);
    ~SigmaMQTT();
    void Subscribe(TopicSubscription subscriptionTopic);
    void Unsubscribe(String topic);
    // bool IsReady();

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
    bool shouldConnect = false;
    esp_mqtt_client_handle_t mqttClient;
    static void onMqttEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    static void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
};

#endif