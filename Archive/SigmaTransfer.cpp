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
    TLogger->Append("Adding protocol: ").Append(name).Internal();
    protocol->SetName(name);
    protocols[name] = protocol;
    String loopName = name + "_event_loop";

    esp_event_loop_args_t loop_args = {
        .queue_size = 100,
        .task_name = loopName.c_str(),
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = 0};

    esp_event_loop_handle_t eventLoop = nullptr;
    esp_err_t espErr = esp_event_loop_create(&loop_args, &eventLoop);
    if (espErr != ESP_OK)
    {
        TLogger->Printf("Failed to create event loop: %d", espErr).Internal();
    }
    protocol->SetEventLoop(eventLoop);
    return true;
}

bool SigmaTransfer::isWiFiRequired()
{
    for (auto &p : protocols)
    {
        TLogger->Append("Checking protocol: ").Append(p.first).Internal();
        if (p.second->IsNetworkRequired())
        {
            TLogger->Append("Protocol: ").Append(p.first).Append(" requires network").Info();
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
    TLogger->Append("Starting").Info();
    if (isWiFiRequired())
    {
        TLogger->Append("WiFi required").Info();
        StartWiFi();
        TLogger->Append("Waiting for WiFi connection").Info();
        int res = WiFi.waitForConnectResult();
        TLogger->Append("WiFi connection result: ").Append(res).Info();
    }
    for (auto &p : protocols)
    {
        TLogger->Append("Starting protocol: ").Append(p.first).Info();
        if (p.second->BeginSetup())
        {
            TLogger->Append("Protocol: ").Append(p.first).Append(" connected").Info();
        }
        else
        {
            TLogger->Append("Protocol: ").Append(p.first).Append(" failed to connect").Error();
        }
    }
    for (auto &p : protocols)
    {
        if (p.second->FinalizeSetup())
        {
            TLogger->Append("Protocol: ").Append(p.first).Append(" finalized setup").Info();
        }
        else
        {
            TLogger->Append("Protocol: ").Append(p.first).Append(" failed to finalize setup").Error();
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
    TLogger->Append("WiFi disconnected").Info();
}

void SigmaTransfer::StartWiFi()
{
    TLogger->Append("Connecting to WiFi network: ").Append(ssid).Internal();
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info)
                 {
        //PLogger->Append("WiFi event: ").Append(event).Internal();
        switch (event)
        {
        case SYSTEM_EVENT_STA_GOT_IP:
        {
            xTimerStop(wifiReconnectTimer, portMAX_DELAY);
            TLogger->Append("WiFi connected. IP address:").Append(WiFi.localIP().toString()).Info();
            for (auto &p : protocols)
            {
                if (p.second->IsNetworkRequired()) {
                    if (p.second->GetShouldConnect()) {
                    p.second->Connect();
                    }
                }
            }
            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            TLogger->Append("WiFi disconnected").Error();
            TLogger->Append("WiFi connection error:").Append(info.wifi_sta_disconnected.reason).Error();
            for (auto &p : protocols)
            {
                if (p.second->IsNetworkRequired()) {
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
            TLogger->Append("WiFi event: ").Append(event).Internal();
            break;
        }
        } });
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), wifiPassword.c_str());
}

SigmaProtocol *SigmaTransfer::GetProtocol(String name)
{
    return protocols[name];
}
