#ifndef SIGMAWIFI_H
#define SIGMAWIFI_H

#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_event.h>
#include "SigmaLoger.h"
#include "SigmaTransferDefs.h"



class SigmaWiFi
{
public:
    SigmaWiFi(WiFiConfig config, SigmaLoger *log = nullptr);
    ~SigmaWiFi();
    void Connect();
    void Disconnect();
    bool IsConnected() { return isConnected; };

private:
    SigmaLoger *Log;
    inline static WiFiConfig config;
    wifi_mode_t mode;
    TimerHandle_t wifiStaReconnectTimer;
    bool isConnected = false;

    static void reconnectWiFiSta(TimerHandle_t xTimer);
 };

#endif