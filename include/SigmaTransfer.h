#ifndef SIGMATRANSFER_H
#define SIGMATRANSFER_H
#pragma once
#include <map>
#include "SigmaProtocol.h"
#include "SigmaLoger.h"
#include "SigmaChannel.h"

class SigmaTransfer
{
public:
    SigmaTransfer(String ssid="", String password="");
    ~SigmaTransfer();
    bool AddProtocol(String name, SigmaProtocol *protocol);
    bool AddChannel(SigmaChannel *channel);
    bool Begin();

private:
    inline static SigmaLoger *PLogger = new SigmaLoger(512);
    String ssid = "";
    String wifiPassword = "";

    std::map<String, SigmaProtocol *> protocols;
    std::map<String, SigmaChannel *> channels;

    void startWiFi(String ssid, String password);
    bool isWiFiRequired();
};

#endif