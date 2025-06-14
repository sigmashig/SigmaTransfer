#include "SigmaProtocol.h"
#include <WiFi.h>
#include <esp_event.h>
#include <SigmaMQTT.h>
#include <SigmaWSServer.h>
#include <SigmaWSClient.h>

SigmaProtocol::SigmaProtocol(String name, SigmaLoger *logger, uint priority, uint queueSize, uint stackSize, uint coreId)
{
    this->Log = (logger != nullptr) ? logger : new SigmaLoger(0);
    this->name = name;
    //messages = std::list<Message>();
    //queueMutex = xSemaphoreCreateMutex();
    esp_event_loop_args_t loop_args = {
        .queue_size = (int32_t)queueSize,
        .task_name = name.c_str(),
        .task_priority = priority,
        .task_stack_size = stackSize,
        .task_core_id = (int)coreId};

    esp_err_t espErr = esp_event_loop_create(&loop_args, &eventLoop);
    if (espErr != ESP_OK)
    {
        Log->Printf("Failed to create event loop: %d", espErr).Internal();
        exit(1);
    }
}

SigmaProtocol::~SigmaProtocol()
{
    esp_event_loop_delete(eventLoop);
}

bool SigmaProtocol::IsIP(String URL)
{
    for (int i = 0; i < URL.length(); i++)
    {
        if (URL[i] == '.' || (URL[i] >= '0' && URL[i] <= '9'))
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}

SigmaProtocol *SigmaProtocol::Create(SigmaProtocolType type, SigmaProtocolConfig config, SigmaLoger *logger, uint priority)
{
    switch (type)
    {
        case SIGMA_PROTOCOL_MQTT:
            return new SigmaMQTT(config.mqttConfig, logger, priority);
            break;
        case SIGMA_PROTOCOL_WS_SERVER:
            return new SigmaWsServer(config.wsServerConfig, logger, priority);
            break;
        case SIGMA_PROTOCOL_WS_CLIENT:
            return new SigmaWsClient(config.wsClientConfig, logger, priority);
            break;
        default:
            return nullptr;
    }
    return nullptr;
}

TopicSubscription *SigmaProtocol::GetSubscription(String topic)
{
    auto it = subscriptions.find(topic);
    if (it != subscriptions.end())
    {
        return &it->second;
    }
    return nullptr;
}

void SigmaProtocol::addSubscription(TopicSubscription subscription)
{
    subscriptions[subscription.topic] = subscription;
}

void SigmaProtocol::removeSubscription(String topic)
{
    subscriptions.erase(topic);
}

SigmaProtocolType SigmaProtocol::String2Type(String typeName)
{
    if (typeName == "MQTT")
    {
        return SIGMA_PROTOCOL_MQTT;
    }
    else if (typeName == "WS_SERVER")
    {
        return SIGMA_PROTOCOL_WS_SERVER;
    }
    else if (typeName == "WS_CLIENT")
    {
        return SIGMA_PROTOCOL_WS_CLIENT;
    }
    return SIGMA_PROTOCOL_UNKNOWN;
}
