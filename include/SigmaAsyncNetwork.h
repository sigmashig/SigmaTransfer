#ifndef SIGMAASYNCNETWORK_H
#define SIGMAASYNCNETWORK_H

#pragma once
#include "Arduino.h"
#include <esp_event.h>
#include <esp_event.h>
#include "SigmaLoger.h"
#include "SigmaTransferDefs.h"


class SigmaAsyncNetwork
{
public:
    SigmaAsyncNetwork(NetworkConfig config, SigmaLoger *log = nullptr);
    ~SigmaAsyncNetwork();
    void Connect();
    void Disconnect();
    static bool IsConnected() { return isConnected; };
    static esp_event_loop_handle_t GetEventLoop() { return eventLoop; };
    static esp_event_base_t GetEventBase() { return eventBase; };

private:
    SigmaLoger *Log;
    inline static NetworkConfig config;
    inline static bool isConnected = false;
    bool shouldConnect = true;
    wifi_mode_t mode = WIFI_MODE_STA;

    TimerHandle_t wifiStaReconnectTimer;
    inline static esp_event_loop_handle_t eventLoop;
    inline static esp_event_base_t eventBase = "SigmaAsyncNetwork";

    void startWiFiSta();
    static void reconnectWiFiSta(TimerHandle_t xTimer);
    esp_err_t postEvent(int32_t eventId, void *eventData = nullptr, size_t eventDataSize = 0);
};

#endif