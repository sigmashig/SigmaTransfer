#ifndef SIGMAASYNCNETWORK_H
#define SIGMAASYNCNETWORK_H

#pragma once
#include "Arduino.h"
#include <esp_event.h>
#include <esp_event.h>
#include "SigmaLoger.h"
#include "SigmaTransferDefs.h"
#include "SigmaWiFi.h"
#include "SigmaEthernet.h"

class SigmaAsyncNetwork
{
public:
    SigmaAsyncNetwork(NetworkConfig config, SigmaLoger *log = nullptr);
    ~SigmaAsyncNetwork();
    void Connect();
    void Disconnect();
    static bool IsConnected();
    static SigmaWiFi *GetWiFi() { return wifi; };
    static bool IsWiFiConnected() { return wifi != nullptr && wifi->IsConnected(); };
    static SigmaEthernet *GetEthernet() { return ethernet; };
    static bool IsEthernetConnected() { return ethernet != nullptr && ethernet->IsConnected(); };
    //static esp_event_loop_handle_t GetEventLoop() { return eventLoop; };
    //static esp_event_base_t GetEventBase() { return eventBase; };
    static esp_err_t PostEvent(int32_t eventId, void *eventData = nullptr, size_t eventDataSize = 0);
    static esp_err_t RegisterEventHandlers(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);
    static bool IsWanConnected() { return numberOfWanConnections > 0; };
    static bool IsLanConnected() { return numberOfLanConnections > 0; };

private:
    SigmaLoger *Log;
    inline static byte numberOfWanConnections = 0;
    inline static byte numberOfLanConnections = 0;
    inline static SigmaWiFi *wifi = nullptr;
    inline static SigmaEthernet *ethernet = nullptr;
    inline static NetworkConfig config;
    inline static bool isConnected = false;
    bool shouldConnect = true;
    wifi_mode_t mode = WIFI_MODE_STA;

    TimerHandle_t wifiStaReconnectTimer;
    inline static esp_event_loop_handle_t eventLoop;
    inline static esp_event_base_t eventBase = "SigmaAsyncNetwork";
};

#endif