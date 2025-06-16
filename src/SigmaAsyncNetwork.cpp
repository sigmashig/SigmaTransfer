#include "SigmaAsyncNetwork.h"
#include "WiFi.h"

ESP_EVENT_DEFINE_BASE(SIGMAASYNCNETWORK_EVENT);

SigmaAsyncNetwork::SigmaAsyncNetwork(NetworkConfig config, SigmaLoger *log)
{
    Log = log != nullptr ? log : new SigmaLoger(0);
    this->config = config;
    if (config.enabled)
    {
        if (config.ethernetConfig.enabled)
        {
            // TODO: Configure Ethernet
        }
        if (config.wifiConfig.enabled)
        {
            mode = config.wifiConfig.wifiMode;
            if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)
            {
                wifiStaReconnectTimer = xTimerCreate("wifiStaTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(reconnectWiFiSta));
            }
            else
            {
                // mode == WIFI_MODE_AP
                wifiStaReconnectTimer = nullptr;
            }
        }
        esp_event_loop_args_t loop_args = {
            .queue_size = 100,
            .task_name = "SigmaAsyncNetwork_event_loop",
            .task_priority = 100, // high priority
            .task_stack_size = 4096,
            .task_core_id = 0};
        esp_err_t espErr = ESP_OK;
        espErr = esp_event_loop_create(&loop_args, &eventLoop);
        if (espErr != ESP_OK)
        {
            Log->Printf("Failed to create event loop: %d", espErr).Internal();
            exit(1);
        }
    }
}

SigmaAsyncNetwork::~SigmaAsyncNetwork()
{
}

void SigmaAsyncNetwork::reconnectWiFiSta(TimerHandle_t xTimer)
{
    WiFi.begin(config.wifiConfig.wifiSta.ssid.c_str(), config.wifiConfig.wifiSta.password.c_str());
}

void SigmaAsyncNetwork::Connect()
{
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA)
    {
        startWiFiSta();
    }
}

void SigmaAsyncNetwork::Disconnect()
{
}

esp_err_t SigmaAsyncNetwork::postEvent(int32_t eventId, void *eventData, size_t eventDataSize)
{
    return esp_event_post_to(eventLoop, SIGMAASYNCNETWORK_EVENT, eventId, eventData, eventDataSize, portMAX_DELAY);
}
void SigmaAsyncNetwork::startWiFiSta()
{
    Serial.println("startWiFiSta");
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
            postEvent(PROTOCOL_STA_CONNECTED, &ip, sizeof(ip));
            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED:
        {
            if (isConnected) {
                Log->Append("WiFi disconnected:").Append(info.wifi_sta_disconnected.reason).Error();
                postEvent(PROTOCOL_STA_DISCONNECTED, &info.wifi_sta_disconnected.reason, sizeof(info.wifi_sta_disconnected.reason)+1);
            }
            isConnected = false;
            if (shouldConnect) {
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
//            Log->Append("WiFi event: ").Append(event).Internal();
            break;
        }
        } });
    // Log->Append("Connecting to network STA:").Append(config.wifiConfig.wifiSta.ssid).Append(":").Append(config.wifiConfig.wifiSta.password).Internal();
    Serial.println("startWiFiSta 1");
    WiFi.mode(WIFI_STA);
    Serial.println("startWiFiSta 2");
    Serial.printf("Connecting to network STA: %s, %s\n", config.wifiConfig.wifiSta.ssid.c_str(), config.wifiConfig.wifiSta.password.c_str());
    WiFi.begin(config.wifiConfig.wifiSta.ssid.c_str(), config.wifiConfig.wifiSta.password.c_str());
    Serial.println("startWiFiSta end");
    // Log->Append("Connecting to network STA end").Internal();
}
