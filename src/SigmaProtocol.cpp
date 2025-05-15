#include "SigmaProtocol.h"
#include <WiFi.h>

ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

SigmaProtocol::SigmaProtocol()
{
    messages = std::list<Message>();
    queueMutex = xSemaphoreCreateMutex();
}

SigmaProtocol::~SigmaProtocol()
{
}

void SigmaProtocol::Sent(String topic, String payload)
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
        Publish(topic, payload);
    }
}

esp_err_t SigmaProtocol::PostEvent(int32_t event_id, void *event_data, size_t size)
{
    Serial.printf("Posting event: %d\n", event_id);
    if (eventLoop == nullptr)
    {
        Serial.printf("Event loop is nullptr\n");
        return esp_event_post(SIGMATRANSFER_EVENT, event_id, event_data, size, portMAX_DELAY);
    }
    else
    {
        Serial.printf("Event loop is not nullptr\n");
        return esp_event_post_to(eventLoop, SIGMATRANSFER_EVENT, event_id, event_data, size, portMAX_DELAY);
    }
}
