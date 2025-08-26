#include "SigmaNetworkMgr.h"
#include <esp_netif.h>
#include <esp_event.h>
// #include "WiFi.h"
#include "SigmaWiFi.h"

SigmaNetworkMgr::SigmaNetworkMgr(NetworkConfig config, SigmaLoger *log)
{
    Log = log != nullptr ? log : new SigmaLoger(0);
    this->config = config;
    esp_err_t espErr = ESP_OK;
    Log->Append("SigmaNetworkMgr init").Internal();
    if (config.enabled)
    {
            
        espErr = esp_netif_init();
        if (espErr != ESP_OK && espErr != ESP_ERR_INVALID_STATE)
        {
            Log->Printf("Failed to initialize esp_netif: %d", espErr).Internal();
        }
        
        esp_event_loop_args_t loop_args = {
            .queue_size = 100,
            .task_name = "SigmaNetworkMgr_event_loop",
            .task_priority = 100, // high priority
            .task_stack_size = 4096,
            .task_core_id = 0};
        espErr = ESP_OK;
        espErr = esp_event_loop_create(&loop_args, &eventLoop);
        if (espErr != ESP_OK)
        {
            Log->Printf("Failed to create event loop: %d", espErr).Internal();
            // exit(1);
        }
        Log->Append("Event loop created").Internal();
        if (config.ethernetConfig.enabled)
        {
            ethernet = new SigmaEthernet(config.ethernetConfig, Log);
        }
        else
        {
            Log->Append("Ethernet is disabled").Error();
            ethernet = nullptr;
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
    }
}

SigmaNetworkMgr::~SigmaNetworkMgr()
{
}

void SigmaNetworkMgr::Connect()
{
    if (ethernet != nullptr)
    {
        ethernet->Connect();
    }
    if (wifi != nullptr)
    {
        wifi->Connect();
    }
}

void SigmaNetworkMgr::Disconnect()
{
    if (wifi != nullptr)
    {
        wifi->Disconnect();
    }
    if (ethernet != nullptr)
    {
        ethernet->Disconnect();
    }
}

bool SigmaNetworkMgr::IsConnected()
{
    bool isConnected = false;
    if (wifi != nullptr)
    {
        isConnected = wifi->IsConnected();
    }
    if (ethernet != nullptr)
    {
        isConnected = isConnected || ethernet->IsConnected();
    }
    return isConnected;
}

esp_err_t SigmaNetworkMgr::PostEvent(int32_t eventId, void *eventData, size_t eventDataSize)
{
    // The events STA/AP/ETHERNET are used for internal purposes. Only LAN/WAN are used for external purposes.
    esp_err_t espErr1 = ESP_OK;
    esp_err_t espErr2 = ESP_OK;
    if (eventId == NETWORK_STA_CONNECTED || eventId == NETWORK_ETHERNET_CONNECTED)
    {
        numberOfWanConnections++;
        numberOfLanConnections++;
        if (numberOfWanConnections == 1)
        {
            espErr1 = esp_event_post_to(eventLoop, eventBase, NETWORK_WAN_CONNECTED, eventData, eventDataSize, portMAX_DELAY);
        }
        if (numberOfLanConnections == 1)
        {
            espErr2 = esp_event_post_to(eventLoop, eventBase, NETWORK_LAN_CONNECTED, eventData, eventDataSize, portMAX_DELAY);
        }
    }
    else if (eventId == NETWORK_STA_DISCONNECTED || eventId == NETWORK_ETHERNET_DISCONNECTED)
    {
         if (numberOfWanConnections > 0)
        {
            numberOfWanConnections--;
        }
        if (numberOfLanConnections > 0)
        {
            numberOfLanConnections--;
        }
        if (numberOfWanConnections == 0)
        {
            espErr1 = esp_event_post_to(eventLoop, eventBase, NETWORK_WAN_DISCONNECTED, eventData, eventDataSize, portMAX_DELAY);
        }
        if (numberOfLanConnections == 0)
        {
            espErr2 = esp_event_post_to(eventLoop, eventBase, NETWORK_LAN_DISCONNECTED, eventData, eventDataSize, portMAX_DELAY);
        }
    }
    else if (eventId == NETWORK_AP_CONNECTED)
    {
        numberOfLanConnections++;
        if (numberOfLanConnections == 1)
        {
            espErr2 = esp_event_post_to(eventLoop, eventBase, NETWORK_LAN_CONNECTED, eventData, eventDataSize, portMAX_DELAY);
        }
    }
    else if (eventId == NETWORK_AP_DISCONNECTED)
    {
        if (numberOfLanConnections > 0)
        {
            numberOfLanConnections--;
        }
        if (numberOfLanConnections == 0)
        {
            espErr2 = esp_event_post_to(eventLoop, eventBase, NETWORK_LAN_DISCONNECTED, eventData, eventDataSize, portMAX_DELAY);
        }
    }
    else
    {
        espErr1 = esp_event_post_to(eventLoop, eventBase, eventId, eventData, eventDataSize, portMAX_DELAY);
    }

    return espErr1 != ESP_OK || espErr2 != ESP_OK ? ESP_FAIL : ESP_OK;
}

esp_err_t SigmaNetworkMgr::RegisterEventHandlers(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg)
{
    return esp_event_handler_register_with(eventLoop, eventBase, event_id, event_handler, event_handler_arg);
}
