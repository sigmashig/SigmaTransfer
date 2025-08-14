#ifndef SIGMANETWORK_H
#define SIGMANETWORK_H

#pragma once

#include <Arduino.h>
#include <SigmaTransferDefs.h>
#include "SigmaLoger.h"

class SigmaNetwork
{
public:
    SigmaNetwork(SigmaLoger *log = nullptr);
    ~SigmaNetwork();

    virtual bool Connect() = 0;
    virtual bool Disconnect() = 0;
    virtual bool IsConnected() { return isConnected; };
    // Generate a MAC address from a given ID. If ID is 0xFF, a random MAC address is generated.
    // The generated MAC address is stored in the mac array.
    static void GenerateMac(byte mac[6], byte id = 0xFF);
    virtual bool IsGotIp() const { return isGotIp; }
    virtual bool IsLinkUp() const { return isLinkUp; }
    static String MACToString(const byte mac[6]);
    static void StringToMAC(String macString, byte mac[6]);
    static EthernetHardwareType EthernetHardwareTypeFromString(String hardwareType);

protected:
    bool isConnected = false;
    bool isGotIp = false;
    bool isLinkUp = false;
    SigmaLoger *Log;
};

#endif