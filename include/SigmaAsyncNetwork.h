#ifndef SIGMAASYNCNETWORK_H
#define SIGMAASYNCNETWORK_H

#pragma once
#include "Arduino.h"
#include <esp_event.h>
#include <esp_event.h>
#include "SigmaLoger.h"

ESP_EVENT_DECLARE_BASE(SIGMAASYNCNETWORK_EVENT);
enum
{
    PROTOCOL_CONNECTED = 0x1000,
    PROTOCOL_DISCONNECTED,
    PROTOCOL_AP_CONNECTED,
    PROTOCOL_STA_CONNECTED,
    PROTOCOL_AP_DISCONNECTED,
    PROTOCOL_STA_DISCONNECTED,
    PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE,
    PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE,
    PROTOCOL_RECEIVED_SIGMA_MESSAGE,
    PROTOCOL_SEND_RAW_TEXT_MESSAGE,
    PROTOCOL_SEND_RAW_BINARY_MESSAGE,
    PROTOCOL_SEND_SIGMA_MESSAGE,
    PROTOCOL_ERROR,
    PROTOCOL_SEND_PING,
    PROTOCOL_SEND_PONG,
    PROTOCOL_RECEIVED_PING,
    PROTOCOL_RECEIVED_PONG,
} EVENT_IDS;

typedef enum
{
    SIGMAASYNCNETWORK_MODE_NONE = 0,
    SIGMAASYNCNETWORK_MODE_STA,
    SIGMAASYNCNETWORK_MODE_AP,
    SIGMAASYNCNETWORK_MODE_STA_AP,

} WorkMode;

typedef struct
{
    String ssid;
    String password;
} WiFiConfigSta;

class SigmaAsyncNetwork
{
public:
    SigmaAsyncNetwork(WiFiConfigSta config);
    ~SigmaAsyncNetwork();
    void Connect();
    void Disconnect();
    static bool IsConnected() { return isConnected; };
    static esp_event_loop_handle_t GetEventLoop() { return eventLoop; };

private:
    SigmaLoger *Log = new SigmaLoger(512);
    inline static WiFiConfigSta configWiFi;
    inline static bool isConnected = false;
    bool shouldConnect = true;
    WorkMode mode = SIGMAASYNCNETWORK_MODE_NONE;

    TimerHandle_t wifiStaReconnectTimer;
    inline static esp_event_loop_handle_t eventLoop;

    void startWiFiSta();
    static void reconnectWiFiSta(TimerHandle_t xTimer);
    esp_err_t postEvent(int32_t eventId, void *eventData = nullptr, size_t eventDataSize = 0);
};

#endif