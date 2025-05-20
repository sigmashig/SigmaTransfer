#include "SigmaAsyncNetwork.h"
#include "WiFi.h"

ESP_EVENT_DEFINE_BASE(SIGMAASYNCNETWORK_EVENT);
SigmaAsyncNetwork::SigmaAsyncNetwork(WiFiConfigSta config) : mode(SIGMAASYNCNETWORK_MODE_STA)
{
    configWiFi = config;
    wifiStaReconnectTimer = xTimerCreate("wifiStaTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(reconnectWiFiSta));
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

SigmaAsyncNetwork::~SigmaAsyncNetwork()
{
}

void SigmaAsyncNetwork::reconnectWiFiSta(TimerHandle_t xTimer)
{
    //startWiFiSta();
    WiFi.begin(configWiFi.ssid.c_str(), configWiFi.password.c_str());
}
void SigmaAsyncNetwork::Connect()
{
    if (mode == SIGMAASYNCNETWORK_MODE_STA)
    {
        startWiFiSta();
        WiFi.begin(configWiFi.ssid.c_str(), configWiFi.password.c_str());
    }
}

esp_err_t SigmaAsyncNetwork::postEvent(int32_t eventId, void *eventData, size_t eventDataSize)
{
    return esp_event_post_to(eventLoop, SIGMAASYNCNETWORK_EVENT, eventId, eventData, eventDataSize, portMAX_DELAY);
}
void SigmaAsyncNetwork::startWiFiSta()
{
    Log->Append("Connecting to WiFi network(STA mode): ").Append(configWiFi.ssid).Internal();
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info)
                 {
        //PLogger->Append("WiFi event: ").Append(event).Internal();
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
            Log->Append("WiFi disconnected").Error();
            Log->Append("WiFi connection error:").Append(info.wifi_sta_disconnected.reason).Error();
            postEvent(PROTOCOL_STA_DISCONNECTED, &info.wifi_sta_disconnected.reason, sizeof(info.wifi_sta_disconnected.reason));
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
            Log->Append("WiFi [0,2,4] event: ").Append(event).Internal();
            break;
        }
        default:
        {
            Log->Append("WiFi event: ").Append(event).Internal();
            break;
        }
        } });
    WiFi.mode(WIFI_STA);
    WiFi.begin(configWiFi.ssid.c_str(), configWiFi.password.c_str());
}
