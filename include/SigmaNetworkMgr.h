#ifndef SIGMANETWORKMGR_H
#define SIGMANETWORKMGR_H

#pragma once
#include "Arduino.h"
#include <esp_event.h>
#include <esp_event.h>
#include "SigmaLoger.h"
#include "SigmaTransferDefs.h"
#include "SigmaNetwork.h"

class SigmaNetworkMgr
{
public:
    static SigmaNetwork *AddNetwork(const NetworkConfig &config);
    /*
    * Add multiple networks. This method is not tested deeply. Please use it with caution.
    */ 
    static bool AddNetworks(const std::vector<NetworkConfig> &configs);
     
    static bool RemoveNetwork(String name);
    static bool RemoveNetworks(std::vector<String> names);
    static bool Connect();
    static bool Disconnect();
    static bool IsConnected(String name = "");
    static bool IsWanConnected() { return numberOfWanConnections > 0; };
    static bool IsLanConnected() { return numberOfLanConnections > 0; };
    static esp_err_t PostEvent(int32_t eventId, void *eventData = nullptr, size_t eventDataSize = 0);
    static esp_err_t RegisterEventHandlerInstance(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance);
    static esp_err_t UnRegisterEventHandlerInstance(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg, esp_event_handler_instance_t *instance);
    static esp_netif_t *GetNetif(String name);
    static SigmaLoger *GetLog() { return Log; };
    static void SetLog(SigmaLoger *log);
    static IPAddress GetIpAddress(String name);

private:
    inline static SigmaLoger *Log = nullptr;
    inline static byte numberOfWanConnections = 0;
    inline static byte numberOfLanConnections = 0;
    inline static std::map<String, SigmaNetwork *> networks;
    inline static bool isInitialized = false;

    //  TimerHandle_t wifiStaReconnectTimer;
    inline static esp_event_loop_handle_t eventLoop = nullptr;
    inline static esp_event_base_t eventBase = "SigmaNetworkMgr";

    static bool init();
};

#endif