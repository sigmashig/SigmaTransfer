#include "SigmaTransfer.h"
#include "SigmaProtocol.h"
#include <WiFi.h>

ESP_EVENT_DEFINE_BASE(SIGMATRANSFER_EVENT);

SigmaTransfer::SigmaTransfer(String ssid, String password)
{
    this->ssid = ssid;
    this->wifiPassword = password;
    wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(ConnectToWifi));
}

SigmaTransfer::~SigmaTransfer()
{
}

bool SigmaTransfer::AddProtocol(String name, SigmaProtocol *protocol)
{
    protocol->SetName(name);
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

void SigmaTransfer::ConnectToWifi()
{
    WiFi.begin(ssid.c_str(), wifiPassword.c_str());
}

bool SigmaTransfer::Begin()
{
    PLogger->Append("Starting").Info();
    if (isWiFiRequired())
    {
        PLogger->Append("WiFi required").Info();
        StartWiFi();
        PLogger->Append("Waiting for WiFi connection").Info();
        int res = WiFi.waitForConnectResult();
        PLogger->Append("WiFi connection result: ").Append(res).Info();
    }
    for (auto &p : protocols)
    {
        PLogger->Append("Starting protocol: ").Append(p.first).Info();
        if (p.second->BeginSetup())
        {
            PLogger->Append("Protocol: ").Append(p.first).Append(" connected").Info();
        }
        else
        {
            PLogger->Append("Protocol: ").Append(p.first).Append(" failed to connect").Error();
        }
    }
    for (auto &p : protocols)
    {
        if (p.second->FinalizeSetup())
        {
            PLogger->Append("Protocol: ").Append(p.first).Append(" finalized setup").Info();
        }
        else
        {
            PLogger->Append("Protocol: ").Append(p.first).Append(" failed to finalize setup").Error();
        }
    }
    return true;
}

void SigmaTransfer::StopWiFi()
{
    isWiFiStopped = true;
    xTimerStop(wifiReconnectTimer, portMAX_DELAY);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    PLogger->Append("WiFi disconnected").Info();
}

void SigmaTransfer::StartWiFi()
{
    PLogger->Append("Connecting to WiFi network: ").Append(ssid).Internal();
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info)
                 {
        //PLogger->Append("WiFi event: ").Append(event).Internal();
        switch (event)
        {
        case SYSTEM_EVENT_STA_GOT_IP:
        {
            xTimerStop(wifiReconnectTimer, portMAX_DELAY);
            PLogger->Append("WiFi connected. IP address:").Append(WiFi.localIP().toString()).Info();
            for (auto &p : protocols)
            {
                if (p.second->IsWiFiRequired()) {
                    if (p.second->GetShouldConnect()) {
                    p.second->Connect();
                    }
                }
            }
            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            PLogger->Append("WiFi disconnected").Error();
            PLogger->Append("WiFi connection error:").Append(info.wifi_sta_disconnected.reason).Error();
            for (auto &p : protocols)
            {
                if (p.second->IsWiFiRequired()) {
                    p.second->Disconnect();
                }
            }
            if (!isWiFiStopped) {
                xTimerStart(wifiReconnectTimer, portMAX_DELAY);
            }
            break;
        }
        case SYSTEM_EVENT_WIFI_READY:
        case SYSTEM_EVENT_STA_START:
        case SYSTEM_EVENT_STA_CONNECTED:
        { // known event - do nothing
            break;
        }
        default:
        {
            PLogger->Append("WiFi event: ").Append(event).Internal();
            break;
        }
        } });
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), wifiPassword.c_str());
}
