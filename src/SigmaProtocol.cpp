#include "SigmaProtocol.h"
#include <WiFi.h>
#include <esp_event.h>

SigmaProtocol::SigmaProtocol()
{
    //messages = std::list<Message>();
    //queueMutex = xSemaphoreCreateMutex();
    esp_err_t espErr = esp_event_loop_create(&loop_args, &eventLoop);
    if (espErr != ESP_OK)
    {
        Log->Printf("Failed to create event loop: %d", espErr).Internal();
        exit(1);
    }
}

SigmaProtocol::~SigmaProtocol()
{
}
/*
esp_err_t SigmaProtocol::PostEvent(int32_t eventId, void *eventData, size_t eventDataSize)
{
    if (eventLoop == nullptr)
    {
        return esp_event_post(SIGMAPROTOCOL_EVENT, eventId, eventData, eventDataSize, portMAX_DELAY);
    }
    else
    {
        return esp_event_post_to(eventLoop, SIGMAPROTOCOL_EVENT, eventId, eventData, eventDataSize, portMAX_DELAY);
    }
}
*/
/*
void SigmaProtocol::Send(String topic, String payload)
{
    if (!IsReady())
    {
        PLogger->Append("Protocol: ").Append(GetName()).Append(" is not ready").Info();
        if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            Message message;
            message.protocolName = GetName();
            message.payload = payload;
            message.topic = rootTopic + topic;
            messages.push_back(message);
            xSemaphoreGive(queueMutex);
        }
    }
    else
    {
        send(topic, payload);
    }
}
*/
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

TopicSubscription *SigmaProtocol::GetSubscription(String topic)
{
    auto it = subscriptions.find(topic);
    if (it != subscriptions.end())
    {
        return &it->second;
    }
    return nullptr;
}
