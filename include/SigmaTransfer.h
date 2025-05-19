#ifndef SIGMATRANSFER_H
#define SIGMATRANSFER_H
#pragma once
#include <map>
#include "SigmaProtocol.h"
#include "SigmaLoger.h"
#include "SigmaChannel.h"
// #include <esp_event.h>

class SigmaTransfer
{
public:
    SigmaTransfer(String ssid = "", String password = "");
    ~SigmaTransfer();
    bool AddProtocol(String name, SigmaProtocol *protocol);
    bool Begin();
    void StopWiFi();
    void StartWiFi();
    SigmaProtocol *GetProtocol(String name);

private:
    SigmaLoger *TLogger = new SigmaLoger(512);
    inline static String ssid = "";
    inline static String wifiPassword = "";
    bool isWiFiStopped = false;
    TimerHandle_t wifiReconnectTimer;
    std::map<String, SigmaProtocol *> protocols;

    bool isWiFiRequired();
    static void ConnectToWifi();
};

#endif