#include "SigmaWiFi.h"
#include "SigmaNetworkMgr.h"

SigmaWiFi::SigmaWiFi(WiFiConfig config, SigmaLoger *log)
{
    Log = log != nullptr ? log : new SigmaLoger(0);
    this->config = config;
    if (config.wifiMode == WIFI_MODE_STA || config.wifiMode == WIFI_MODE_APSTA)
    {
        wifiStaReconnectTimer = xTimerCreate("wifiStaTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(reconnectWiFiSta));
    }
    else
    {
        // mode == WIFI_MODE_AP
        wifiStaReconnectTimer = nullptr;
    }
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info)
                 {
        switch (event)
        {
        case SYSTEM_EVENT_STA_GOT_IP:
        {
            xTimerStop(wifiStaReconnectTimer, portMAX_DELAY);
            Log->Append("WiFi connected. IP address:").Append(WiFi.localIP().toString()).Info();
            isConnected = true;
            IPAddress ip = WiFi.localIP();
            SigmaNetworkMgr::PostEvent(NETWORK_STA_CONNECTED, &ip, sizeof(ip));
            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            if (isConnected) {
                Log->Append("WiFi disconnected:").Append(info.wifi_sta_disconnected.reason).Error();
                SigmaNetworkMgr::PostEvent(NETWORK_STA_DISCONNECTED, &info.wifi_sta_disconnected.reason, sizeof(info.wifi_sta_disconnected.reason)+1);
            }
            isConnected = false;
            if (this->config.shouldReconnect) {
                xTimerStart(wifiStaReconnectTimer, portMAX_DELAY);
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
            Log->Append("WiFi event: ").Append(event).Internal();
            break;
        }
        } });
}

SigmaWiFi::~SigmaWiFi()
{
}

void SigmaWiFi::reconnectWiFiSta(TimerHandle_t xTimer)
{
    WiFi.begin(config.wifiSta.ssid.c_str(), config.wifiSta.password.c_str());
}

void SigmaWiFi::Connect()
{
    if (config.wifiMode == WIFI_MODE_STA || config.wifiMode == WIFI_MODE_APSTA)
    {
        reconnectWiFiSta(nullptr);
        // WiFi.begin(config.wifiSta.ssid.c_str(), config.wifiSta.password.c_str());
    }
    if (config.wifiMode == WIFI_MODE_AP || config.wifiMode == WIFI_MODE_APSTA)
    {
        // mode == WIFI_MODE_AP
        WiFi.softAP(config.wifiAp.ssid.c_str(), config.wifiAp.password.c_str());
    }
}

void SigmaWiFi::Disconnect()
{
    if (config.wifiMode == WIFI_MODE_STA || config.wifiMode == WIFI_MODE_APSTA)
    {
        WiFi.disconnect();
    }
    if (config.wifiMode == WIFI_MODE_AP || config.wifiMode == WIFI_MODE_APSTA)
    {
        WiFi.softAPdisconnect(true);
    }
}
