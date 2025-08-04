#include "SigmaAsyncNetwork.h"
// #include "WiFi.h"
#include "SigmaWiFi.h"

SigmaAsyncNetwork::SigmaAsyncNetwork(NetworkConfig config, SigmaLoger *log)
{
    Log = log != nullptr ? log : new SigmaLoger(0);
    this->config = config;
    if (config.enabled)
    {
        if (config.ethernetConfig.enabled)
        {
            // TODO: Configure Ethernet
        }
        if (config.wifiConfig.enabled)
        {
            wifi = new SigmaWiFi(config.wifiConfig, Log);
        }
        else
        {
            Log->Append("WiFi is disabled").Error();
            wifi = nullptr;
        }
        esp_event_loop_args_t loop_args = {
            .queue_size = 100,
            .task_name = "SigmaAsyncNetwork_event_loop",
            .task_priority = 100, // high priority
            .task_stack_size = 4096,
            .task_core_id = 0};
        esp_err_t espErr = ESP_OK;
        espErr = esp_event_loop_create(&loop_args, &eventLoop);
        if (espErr != ESP_OK)
        {
            Log->Printf("Failed to create event loop: %d", espErr).Internal();
            exit(1);
        }
    }
}

SigmaAsyncNetwork::~SigmaAsyncNetwork()
{
}

void SigmaAsyncNetwork::Connect()
{
    if (wifi != nullptr)
    {
        wifi->Connect();
    }
}

void SigmaAsyncNetwork::Disconnect()
{
    if (wifi != nullptr)
    {
        wifi->Disconnect();
    }
}

bool SigmaAsyncNetwork::IsConnected()
{
    if (wifi != nullptr)
    {
        return wifi->IsConnected();
    }
    return false;
}

esp_err_t SigmaAsyncNetwork::PostEvent(int32_t eventId, void *eventData, size_t eventDataSize)
{
    return esp_event_post_to(eventLoop, eventBase, eventId, eventData, eventDataSize, portMAX_DELAY);
}
