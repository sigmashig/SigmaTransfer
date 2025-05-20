#ifndef SIGMATRANSFER_H
#define SIGMATRANSFER_H
#pragma once
#include <map>
#include "SigmaProtocol.h"
#include "SigmaLoger.h"
#include "SigmaChannel.h"
// #include <esp_event.h>
ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

struct WiFiConfigSta
{
    String ssid;
    String password;
};

class SigmaTransfer
{
public:
    SigmaTransfer(WiFiConfigSta config);
    ~SigmaTransfer();
    bool AddProtocol(String name, SigmaProtocol *protocol);
    bool Begin();
    void StopWiFi();
    void StartWiFi();
    SigmaProtocol *GetProtocol(String name);

private:
    SigmaLoger *TLogger = new SigmaLoger(512);
    WiFiConfigSta wifiConfig;
    bool isWiFiStopped = false;
    TimerHandle_t wifiReconnectTimer;
    std::map<String, SigmaProtocol *> protocols;

    bool isWiFiRequired();
    static void ConnectToWifi();
};

#endif