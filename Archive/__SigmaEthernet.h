#ifndef SIGMAETHERNET_H
#define SIGMAETHERNET_H

#pragma once
#include <Arduino.h>
#include <esp_event.h>
#include <SPI.h>
#include <Ethernet.h>

#include "SigmaLoger.h"
#include "SigmaTransferDefs.h"

class SigmaEthernet
{
public:
    SigmaEthernet(EthernetConfig config, SigmaLoger *log = nullptr);
    ~SigmaEthernet();

    void Connect();
    void Disconnect();
    bool IsConnected() { return isConnected; };
    static byte *GenerateMac(byte id, byte mac[6]);

private:
    SigmaLoger *Log;
    inline static EthernetConfig config;
    TimerHandle_t ethernetReconnectTimer;
    bool isConnected = false;
    byte mac[6];

    //static void reconnectEthernet(TimerHandle_t xTimer);
};

#endif