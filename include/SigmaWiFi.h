#ifndef SIGMAWIFI_H
#define SIGMAWIFI_H

#pragma once
#include <Arduino.h>
//#include <WiFi.h>
#include <esp_event.h>
#include "SigmaLoger.h"
#include "SigmaTransferDefs.h"
#include "SigmaNetwork.h"



class SigmaWiFi : public SigmaNetwork
{
public:
    SigmaWiFi(WiFiConfig config);
    ~SigmaWiFi();
    bool Connect();
    bool Disconnect();
    bool IsConnected() { return isConnected; };
    bool Begin() override;
    IPAddress GetIpAddress() const override { return ip; };

private:
    WiFiConfig config;
    TimerHandle_t wifiStaReconnectTimer;
    bool isConnected = false;
    IPAddress ip = IPAddress();
    static void onWiFiEvent(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData);

    void handleWiFiEvent(esp_event_base_t eventBase, int32_t eventId, void *eventData);


    static bool reconnectWiFiSta(TimerHandle_t xTimer);
 };

#endif