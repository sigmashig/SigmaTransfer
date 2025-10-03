#include "SigmaWiFi.h"
#include "SigmaNetworkMgr.h"
#include <esp_wifi_default.h>
#include <esp_wifi.h>

SigmaWiFi::SigmaWiFi(WiFiConfig config)
{
    this->config = config;
};

void SigmaWiFi::onWiFiEvent(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    auto *self = static_cast<SigmaWiFi *>(arg);
    self->handleWiFiEvent(eventBase, eventId, eventData);
}

void SigmaWiFi::handleWiFiEvent(esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    SigmaNetworkMgr::GetLog()->Append("WiFi event: " + String(eventBase) + " " + String(eventId)).Error();
    if (eventBase == IP_EVENT)
    {
        if (eventId == IP_EVENT_STA_GOT_IP)
        {
            xTimerStop(wifiStaReconnectTimer, portMAX_DELAY);
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)eventData;
            IPAddress ip = IPAddress(event->ip_info.ip.addr);
            SigmaNetworkMgr::GetLog()->Append("WiFi connected. IP address:").Append(ip.toString()).Info();
            isConnected = true;
            SigmaNetworkMgr::PostEvent(NETWORK_STA_CONNECTED, &ip, sizeof(ip));
        }
    }
    else if (eventBase == WIFI_EVENT)
    {
        switch (eventId)
        {
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            if (isConnected)
            {
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)eventData;
                SigmaNetworkMgr::GetLog()->Append("WiFi STA disconnected:").Error();
                String msg = "Reason" + String(event->reason);
                SigmaNetworkMgr::PostEvent(NETWORK_STA_DISCONNECTED, &msg, msg.length() + 1);
            }
            isConnected = false;
            if (this->config.shouldReconnect)
            {
                xTimerStart(wifiStaReconnectTimer, portMAX_DELAY);
            }
            break;
        }
        }
    }
}

bool SigmaWiFi::Begin()
{
    if (!config.enabled)
    {
        SigmaNetworkMgr::GetLog()->Append("WiFi is disabled").Internal();
        return false;
    }

    esp_err_t err;
    SigmaNetworkMgr::Init(); // init event loop and netif
    SigmaNetworkMgr::GetLog()->Append("WiFi Init").Internal();

    if (config.wifiMode == WIFI_MODE_STA || config.wifiMode == WIFI_MODE_APSTA)
    {
        wifiStaReconnectTimer = xTimerCreate("wifiStaTimer", pdMS_TO_TICKS(2000), pdFALSE, this, reinterpret_cast<TimerCallbackFunction_t>(reconnectWiFiSta));
    }
    else
    {
        wifiStaReconnectTimer = nullptr;
    }
    SigmaNetworkMgr::GetLog()->Append("Heap before WiFi init: ").Append(esp_get_free_heap_size()).Internal();
    SigmaNetworkMgr::GetLog()->Append("Initializing WiFi config").Internal();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);

    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Error("Failed to initialize WiFi");
        return false;
    }
    SigmaNetworkMgr::GetLog()->Append("Heap after WiFi init: ").Append(esp_get_free_heap_size()).Internal();
    SigmaNetworkMgr::GetLog()->Append("Registering WiFi event handler").Internal();
    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &onWiFiEvent, this);
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Error("Failed to register WiFi event handler");
        return false;
    }
    SigmaNetworkMgr::GetLog()->Append("Registering IP event handler").Internal();
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &onWiFiEvent, this);
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Error("Failed to register IP event handler");
        return false;
    }
    SigmaNetworkMgr::GetLog()->Append("Setting WiFi mode").Internal();
    err = esp_wifi_set_mode(config.wifiMode);
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Error("Failed to set WiFi mode");
        return false;
    }

    if (config.wifiMode == WIFI_MODE_STA || config.wifiMode == WIFI_MODE_APSTA)
    {
        SigmaNetworkMgr::GetLog()->Append("Creating default WiFi STA netif").Internal();
        netif1 = esp_netif_create_default_wifi_sta();
        if (netif1 == nullptr)
        {
            SigmaNetworkMgr::GetLog()->Error("Failed to create default WiFi STA netif");
            return false;
        }

        if (!config.wifiSta.useDhcp)
        {
            esp_netif_dhcp_status_t dhcpStatus;
            esp_netif_dhcpc_get_status(netif1, &dhcpStatus);
            if (dhcpStatus != ESP_NETIF_DHCP_STOPPED)
            {
                esp_netif_dhcpc_stop(netif1);
            }

            esp_netif_ip_info_t ip_info;
            IPAddress ip = config.wifiSta.ip;
            esp_netif_str_to_ip4(config.wifiSta.ip.toString().c_str(), &ip_info.ip);
            esp_netif_str_to_ip4(config.wifiSta.gateway.toString().c_str(), &ip_info.gw);
            esp_netif_str_to_ip4(config.wifiSta.subnet.toString().c_str(), &ip_info.netmask);

            esp_netif_set_ip_info(netif1, &ip_info);
        }

        wifi_config_t sta_config = {};
        strncpy(reinterpret_cast<char *>(sta_config.sta.ssid), config.wifiSta.ssid.c_str(), sizeof(sta_config.sta.ssid));
        strncpy(reinterpret_cast<char *>(sta_config.sta.password), config.wifiSta.password.c_str(), sizeof(sta_config.sta.password));
        sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        sta_config.sta.pmf_cfg.capable = true;
        sta_config.sta.pmf_cfg.required = false;
        sta_config.sta.pmf_cfg.required = false;
        SigmaNetworkMgr::GetLog()->Append("Setting WiFi STA config").Internal();
        err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        if (err != ESP_OK)
        {
            SigmaNetworkMgr::GetLog()->Error("Failed to set WiFi STA config");
            return false;
        }
    }
    if (config.wifiMode == WIFI_MODE_AP || config.wifiMode == WIFI_MODE_APSTA)
    {
        netif2 = esp_netif_create_default_wifi_ap();
        if (netif2 == nullptr)
        {
            SigmaNetworkMgr::GetLog()->Error("Failed to create default WiFi AP netif");
            return false;
        }
        wifi_config_t ap_config = {};
        strncpy(reinterpret_cast<char *>(ap_config.ap.ssid), config.wifiAp.ssid.c_str(), sizeof(ap_config.ap.ssid));
        strncpy(reinterpret_cast<char *>(ap_config.ap.password), config.wifiAp.password.c_str(), sizeof(ap_config.ap.password));
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        if (err != ESP_OK)
        {
            SigmaNetworkMgr::GetLog()->Error("Failed to set WiFi AP config");
            return false;
        }
    }
    SigmaNetworkMgr::GetLog()->Append("Starting WiFi").Internal();
    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Error("Failed to start WiFi");
        return false;
    }
    SigmaNetworkMgr::GetLog()->Append("WiFi initialized").Internal();
    return Connect();
}
SigmaWiFi::~SigmaWiFi()
{
}

bool SigmaWiFi::reconnectWiFiSta(TimerHandle_t xTimer)
{
    esp_err_t err;
    err = esp_wifi_connect();
    if (err != ESP_OK)
    {
        Serial.printf("ReconnectWiFiSta: Failed to reconnect WiFi STA:(%d)%s\n", err, esp_err_to_name(err));
        SigmaNetworkMgr::GetLog()->Printf("Failed to reconnect WiFi STA:(%d)%s", err, esp_err_to_name(err)).Error();
    }
    return err == ESP_OK;
}

bool SigmaWiFi::Connect()
{
    if (!config.enabled)
    {
        SigmaNetworkMgr::GetLog()->Append("WiFi is disabled").Internal();
        return false;
    }
    if (config.wifiMode == WIFI_MODE_STA || config.wifiMode == WIFI_MODE_APSTA)
    {
        if (!reconnectWiFiSta(wifiStaReconnectTimer))
        {
            SigmaNetworkMgr::GetLog()->Append("(2)Failed to reconnect WiFi STA").Error();
            return false;
        }
    }
    if (config.wifiMode == WIFI_MODE_AP || config.wifiMode == WIFI_MODE_APSTA)
    {
        // Nothing todo - wifi is started in Begin()
    }
    return true;
}

bool SigmaWiFi::Disconnect()
{
    esp_err_t err;
    if (config.wifiMode == WIFI_MODE_STA || config.wifiMode == WIFI_MODE_APSTA)
    {
        err = esp_wifi_disconnect();
        if (err != ESP_OK)
        {
            SigmaNetworkMgr::GetLog()->Error("Failed to disconnect WiFi");
            return false;
        }
    }

    err = esp_wifi_stop();
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Error("Failed to stop WiFi");
        return false;
    }
    return true;
}
