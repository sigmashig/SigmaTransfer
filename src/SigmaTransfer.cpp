#include "SigmaTransfer.h"
#include "SigmaProtocol.h"
#include <WiFi.h>

SigmaTransfer::SigmaTransfer(String ssid, String password) : ssid(ssid), wifiPassword(password)
{
}

SigmaTransfer::~SigmaTransfer()
{
}

bool SigmaTransfer::AddProtocol(String name, SigmaProtocol *protocol)
{
    protocols[name] = protocol;
    return true;
}

bool SigmaTransfer::AddChannel(SigmaChannel *channel)
{
    channels[channel->GetConfig().correspondentId] = channel;
    return true;
}

bool SigmaTransfer::isWiFiRequired()
{
    for (auto &p : protocols)
    {
        if (p.second->IsWiFiRequired())
        {
            return true;
        }
    }
    return false;
}

bool SigmaTransfer::Begin()
{
    PLogger->Append("Starting").Info();
    if (isWiFiRequired())
    {
        PLogger->Append("WiFi required").Info();
        startWiFi(ssid, wifiPassword);
        PLogger->Append("Waiting for WiFi connection").Info();
        int res = WiFi.waitForConnectResult();
        PLogger->Append("WiFi connection result: ").Append(res).Info();
    }
    for (auto &p : protocols)
    {
        PLogger->Append("Starting protocol: ").Append(p.first).Info();
        if (p.second->Begin())
        {
            PLogger->Append("Protocol: ").Append(p.first).Append(" connected").Info();
        }
        else
        {
            PLogger->Append("Protocol: ").Append(p.first).Append(" failed to connect").Error();
        }
    }
    return true;
}

void SigmaTransfer::startWiFi(String ssid, String password)
{
    PLogger->Append(F("Connecting to WiFi network: ")).Append(ssid).Internal();

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info)
                 {
        PLogger->Append("WiFi event: ").Append(event).Internal();
        switch (event)
        {
        case SYSTEM_EVENT_STA_GOT_IP:
        {
            PLogger->Append("WiFi connected. IP address:").Append(WiFi.localIP().toString()).Info();
            for (auto &p : protocols)
            {
                if (p.second->IsWiFiRequired()) {
                    p.second->Connect();
                }
            }
            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            PLogger->Append("WiFi connection error:").Append(info.wifi_sta_disconnected.reason).Error();
            for (auto &p : protocols)
            {
                if (p.second->IsWiFiRequired()) {
                    p.second->Disconnect();
                }
            }
            break;
        }
        } });
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
}
