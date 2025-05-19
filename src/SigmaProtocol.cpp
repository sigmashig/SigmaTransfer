#include "SigmaProtocol.h"
#include <WiFi.h>
#include <esp_event.h>

ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

SigmaProtocol::SigmaProtocol(String loopName)
{
    messages = std::list<Message>();
    queueMutex = xSemaphoreCreateMutex();
    esp_event_loop_args_t loop_args = {
        .queue_size = 100,
        .task_name = loopName.c_str(),
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = 0};

    esp_err_t espErr = esp_event_loop_create(&loop_args, &eventLoop);
    if (espErr != ESP_OK)
    {
        PLogger->Printf("Failed to create event loop: %d", espErr).Internal();
        exit(1);
    }
}

SigmaProtocol::~SigmaProtocol()
{
}

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
        Publish(topic, payload);
    }
}


esp_err_t SigmaProtocol::PostEvent(int32_t event_id, void *event_data, size_t size)
{
        return esp_event_post_to(eventLoop, SIGMATRANSFER_EVENT, event_id, event_data, size, portMAX_DELAY);
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
