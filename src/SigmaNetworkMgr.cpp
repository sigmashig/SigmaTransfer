#include "SigmaNetworkMgr.h"
#include <esp_netif.h>
#include <esp_event.h>
#include "SigmaWiFi.h"
#include "SigmaEthernet.h"

bool SigmaNetworkMgr::init()
{
    if (isInitialized)
    {
        return true;
    }
    esp_err_t espErr = ESP_OK;
    if (Log == nullptr)
    {
        Log = new SigmaLoger(0);
    }
    networks.clear();
    espErr = esp_event_loop_create_default();
    if (espErr != ESP_OK)
    {
        Log->Printf("Failed to create default event loop: %d", espErr).Error();
    }
    Log->Append("Default Event loop created").Internal();

    esp_event_loop_args_t loop_args = {
        .queue_size = 50,
        .task_name = "SigmaNetworkMgr_event_loop",
        .task_priority = 50, // high priority
        .task_stack_size = 4096,
        .task_core_id = 0};
    espErr = esp_event_loop_create(&loop_args, &eventLoop);
    if (espErr != ESP_OK)
    {
        Log->Printf("Failed to create Internal event loop: %d", espErr).Error();
    }

    Log->Append("Initializing esp_netif").Internal();
    espErr = esp_netif_init();
    if (espErr != ESP_OK && espErr != ESP_ERR_INVALID_STATE)
    {
        Log->Printf("Failed to initialize esp_netif: %d", espErr).Error();
    }
    Log->Append("esp_netif initialized").Internal();

    isInitialized = true;
    return true;
}


SigmaNetwork *SigmaNetworkMgr::AddNetwork(const NetworkConfig &config)
{
    if (!isInitialized)
    {
        init();
    }
    if (networks.find(config.name) != networks.end())
    {
        Log->Printf("Network %s already exists", config.name.c_str()).Warn();
    }
    else
    {
        SigmaNetwork *network = nullptr;
        if (config.type == NETWORK_WIFI)
        {
            network = new SigmaWiFi(config.networkConfig.wifiConfig);
        }
        else if (config.type == NETWORK_ETHERNET)
        {
            network = new SigmaEthernet(config.networkConfig.ethernetConfig);
        }
        if (network != nullptr)
        {
            networks[config.name] = network;
            // network->Init();
        }
    }
    return networks[config.name];
}

bool SigmaNetworkMgr::AddNetworks(const std::vector<NetworkConfig> &configs)
{
    bool ret = true;
    for (auto &config : configs)
    {
        if (AddNetwork(config) == nullptr)
        {
            Log->Printf("Failed to add network %s", config.name.c_str()).Error();
            ret = false;
        }
    }
    return ret;
}

bool SigmaNetworkMgr::RemoveNetwork(String name)
{
    bool ret = true;
    if (networks.find(name) != networks.end())
    {
        if (!networks[name]->Disconnect())
        {
            Log->Printf("Failed to disconnect network %s", name.c_str()).Error();
            ret = false;
        }
        networks.erase(name);
    }
    return ret;
}

bool SigmaNetworkMgr::RemoveNetworks(std::vector<String> names)
{
    bool ret = true;
    for (auto &name : names)
    {
        if (!RemoveNetwork(name))
        {
            Log->Printf("Failed to remove network %s", name.c_str()).Error();
            ret = false;
        }
    }
    return ret;
}

bool SigmaNetworkMgr::Connect()
{
    bool ret = true;
    for (auto &config : networks)
    {
        if (config.second->Begin() == false)
        {
            Log->Printf("Failed to begin network: %s", config.first.c_str()).Error();
            ret = false;
        }
    }
    delay(500); // Wait for the network to be initialized
    for (auto &config : networks)
    {
        if (!config.second->Connect())
        {
            Log->Printf("Failed to connect network: %s", config.first.c_str()).Error();
            ret = false;
        }
    }
    return ret;
}

bool SigmaNetworkMgr::Disconnect()
{
    bool ret = true;
    for (auto &config : networks)
    {
        if (!config.second->Disconnect())
        {
            Log->Printf("Failed to disconnect network %s", config.first.c_str()).Error();
            ret = false;
        }
    }
    return ret;
}

bool SigmaNetworkMgr::IsConnected(String name)
{
    if (networks.find(name) != networks.end())
    {
        return networks[name]->IsConnected();
    }
    else
    {
        return false;
    }
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

esp_err_t SigmaNetworkMgr::RegisterEventHandlerInstance(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance)
{
   // return esp_event_handler_register_with(eventLoop, eventBase, event_id, event_handler, event_handler_arg);
   return esp_event_handler_instance_register_with(eventLoop, eventBase, event_id, event_handler, event_handler_arg, instance);
}

esp_err_t SigmaNetworkMgr::UnRegisterEventHandlerInstance(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance)
{
   return esp_event_handler_instance_unregister_with(eventLoop, eventBase, event_id, instance);
}

esp_netif_t *SigmaNetworkMgr::GetNetif(String name)
{
    if (networks.find(name) != networks.end())
    {
        return networks[name]->GetNetif();
    }
    else
    {
        return nullptr;
    }
}

void SigmaNetworkMgr::SetLog(SigmaLoger *log)
{
    if (Log != nullptr)
    {
        delete Log;
    }
    if (log == nullptr)
    {
        Log = new SigmaLoger(0);
    }
    else
    {
        Log = log;
    }
}

IPAddress SigmaNetworkMgr::GetIpAddress(String name)
{
    if (networks.find(name) != networks.end())
    {
        return networks[name]->GetIpAddress();
    }
    else if (name == "" || networks.size() == 1)
    {
        return networks.begin()->second->GetIpAddress();
    }
    else
    {
        Log->Printf("Network %s not found", name.c_str()).Error();
        return IPAddress(0, 0, 0, 0);
    }
}
