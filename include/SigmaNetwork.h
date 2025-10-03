#ifndef SIGMANETWORK_H
#define SIGMANETWORK_H

#pragma once

#include <Arduino.h>
#include <SigmaTransferDefs.h>
#include "SigmaLoger.h"
#include <esp_netif.h>

class SigmaNetwork
{
public:
    virtual bool Connect() = 0;
    virtual bool Disconnect() = 0;
    virtual bool IsConnected() { return isConnected; };
    virtual IPAddress GetIpAddress() const = 0;
    // Generate a MAC address from a given ID. If ID is 0xFF, a random MAC address is generated.
    // The generated MAC address is stored in the mac array.
    virtual bool IsGotIp() const { return isGotIp; }
    virtual bool IsLinkUp() const { return isLinkUp; }
    static String MACToString(const byte mac[6]);
    static void StringToMAC(String macString, byte mac[6]);
    static EthernetHardwareType EthernetHardwareTypeFromString(String hardwareType);
    virtual bool Begin() = 0;
    virtual esp_netif_t *GetNetif() const
    {
        if (netif1 != nullptr)
            return netif1;
        else
            return netif2;
    };
    virtual esp_netif_t *GetNetifFirst() const { return netif1; };
    virtual esp_netif_t *GetNetifSecond() const { return netif2; };
    static void GenerateMac(byte mac[6], byte id = 0xFF);

protected:
    bool isConnected = false;
    bool isGotIp = false;
    bool isLinkUp = false;
    esp_netif_t *netif1 = nullptr;
    esp_netif_t *netif2 = nullptr;

    SigmaNetwork();
    ~SigmaNetwork();
};

#endif