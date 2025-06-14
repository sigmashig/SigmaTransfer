#include "SigmaMQTT.h"
#include <esp_event.h>
#include <WiFi.h>
#include <string.h>
#include <SigmaLoger.h>
#include "SigmaAsyncNetwork.h"

ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

SigmaMQTT::SigmaMQTT(String name, SigmaLoger *logger, MqttConfig config, uint priority) : SigmaProtocol(name, logger, priority)
{
    this->config = config;
    esp_mqtt_client_config_t mqtt_cfg;
    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
    esp_err_t err;

    mqtt_cfg.event_loop_handle = GetEventLoop();

    if (SigmaProtocol::IsIP(config.server))
    {
        IPAddress ip;
        ip.fromString(config.server);
        mqtt_cfg.host = strdup(ip.toString().c_str());
    }
    else
    {
        mqtt_cfg.host = strdup(config.server.c_str());
    }

    mqtt_cfg.port = config.port;
    if (config.username.length() > 0)
    {
        mqtt_cfg.username = strdup(config.username.c_str());
    }
    else
    {
        mqtt_cfg.username = NULL;
    }
    if (config.password.length() > 0)
    {
        mqtt_cfg.password = strdup(config.password.c_str());
    }
    else
    {
        mqtt_cfg.password = NULL;
    }
    if (config.clientId.length() > 0)
    {
        mqtt_cfg.client_id = strdup(config.clientId.c_str());
    }
    else
    {
        mqtt_cfg.client_id = NULL;
    }
    mqtt_cfg.keepalive = config.keepAlive;
    mqtt_cfg.disable_auto_reconnect = false;
    mqtt_cfg.reconnect_timeout_ms = 5000;
    // mqtt_cfg.protocol_ver = MQTT_PROTOCOL_V5;

    mqttClient = esp_mqtt_client_init(&mqtt_cfg);
    err = esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY, onMqttEvent, this);
    if (err != ESP_OK)
    {
        Log->Append("Failed to register MQTT event").Internal();
    }
    free((void *)mqtt_cfg.host);
    if (mqtt_cfg.username)
    {
        free((void *)mqtt_cfg.username);
    }
    if (mqtt_cfg.password)
    {
        free((void *)mqtt_cfg.password);
    }
    if (mqtt_cfg.client_id)
    {
        free((void *)mqtt_cfg.client_id);
    }

    //    esp_mqtt_client_start(mqttClient);
    esp_event_handler_register_with(SigmaAsyncNetwork::GetEventLoop(), SIGMAASYNCNETWORK_EVENT, ESP_EVENT_ANY_ID, networkEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, protocolEventHandler, this);

    Log->Append("MQTT client initialized").Internal();
}

SigmaMQTT::~SigmaMQTT()
{
    // Clean up allocated memory
    if (mqttClient)
    {
        esp_mqtt_client_stop(mqttClient);
        esp_mqtt_client_destroy(mqttClient);
    }
}

void SigmaMQTT::networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaMQTT *mqtt = (SigmaMQTT *)arg;
    mqtt->Log->Append("networkEventHandler:").Append(event_id).Internal();
    if (event_id == PROTOCOL_STA_CONNECTED)
    {
        mqtt->Connect();
    }
    else if (event_id == PROTOCOL_STA_DISCONNECTED)
    {
        mqtt->Disconnect();
    }
}

void SigmaMQTT::protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaMQTT *mqtt = (SigmaMQTT *)arg;
    mqtt->Log->Append("protocolEventHandler:").Append(event_id).Internal();
    // SigmaInternalPkg *pkg = (SigmaInternalPkg *)event_data;
    SigmaInternalPkg pkg((char *)event_data);
    if (event_id == PROTOCOL_SEND_SIGMA_MESSAGE)
    {
        bool res;
        mqtt->publish(&pkg);
    }
}

void SigmaMQTT::Unsubscribe(String topic)
{
    String fullTopic = config.rootTopic + topic;
    esp_mqtt_client_unsubscribe(mqttClient, fullTopic.c_str());
    removeSubscription(fullTopic);
}

void SigmaMQTT::Connect()
{
    // Log->Append("..Connecting to MQTT..").Info();
    esp_mqtt_client_start(mqttClient);
}

void SigmaMQTT::Disconnect()
{
    Log->Append("Disconnecting from MQTT").Info();
    esp_mqtt_client_stop(mqttClient);
}

void SigmaMQTT::Subscribe(TopicSubscription subscriptionTopic)
{
    subscriptionTopic.topic = config.rootTopic + "/" + subscriptionTopic.topic;

    addSubscription(subscriptionTopic);
    Log->Append("Subscribing to:").Append(subscriptionTopic.topic).Internal();
    esp_mqtt_client_subscribe(mqttClient, subscriptionTopic.topic.c_str(), 0);
}

void SigmaMQTT::publish(SigmaInternalPkg *pkg)
{
    esp_mqtt_client_enqueue(mqttClient, (config.rootTopic + pkg->GetTopic()).c_str(), pkg->GetPayload().c_str(), pkg->GetPayload().length() + 1, 0, false, true);
}

void SigmaMQTT::onMqttEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    SigmaMQTT *mqtt = (SigmaMQTT *)handler_args;
    mqtt->Log->Append("onMqttEvent:").Append(event_id).Internal();
    esp_mqtt_event_id_t mqtt_event_id = (esp_mqtt_event_id_t)event_id;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    // esp_mqtt_client_handle_t client = (*event)->client;

    switch (mqtt_event_id)
    {
    case MQTT_EVENT_CONNECTED:
    {
        mqtt->Log->Append("MQTT_EVENT_CONNECTED").Internal();
        mqtt->isReady = true;
        esp_event_post_to(mqtt->GetEventLoop(), mqtt->GetEventBase(), PROTOCOL_CONNECTED, (void *)mqtt->name.c_str(), mqtt->name.length() + 1, portMAX_DELAY);
        for (auto const &x : mqtt->subscriptions)
        {
            if (x.second.isReSubscribe)
            {
                mqtt->Log->Append("Resubscribing to:").Append(x.first).Internal();
                esp_mqtt_client_subscribe(mqtt->mqttClient, x.first.c_str(), 0);
            }
        }

        break;
    }
    case MQTT_EVENT_DISCONNECTED:
    {
        mqtt->Log->Append("MQTT_EVENT_DISCONNECTED").Internal();
        mqtt->isReady = false;
        esp_event_post_to(mqtt->GetEventLoop(), mqtt->GetEventBase(), PROTOCOL_DISCONNECTED, (void *)mqtt->name.c_str(), mqtt->name.length() + 1, portMAX_DELAY);
        break;
    }
    case MQTT_EVENT_DATA:
    {
        mqtt->Log->Append("MQTT_EVENT_DATA").Internal();
        if (event->topic_len == 0)
        {
            mqtt->Log->Append("Topic length is zero. Ignore").Error();
            break;
        }
        char *zTopic;
        zTopic = (char *)malloc(event->topic_len + 1);
        strncpy(zTopic, event->topic, event->topic_len);
        zTopic[event->topic_len] = '\0';

        char *zData;
        if (event->data_len == 0)
        {
            zData = (char *)malloc(1);
            zData[0] = '\0';
        }
        else
        {
            zData = (char *)malloc(event->data_len + 1);
            strncpy(zData, event->data, event->data_len);
            zData[event->data_len] = '\0';
        }
        SigmaInternalPkg pkg(zTopic, zData);
        free(zTopic);
        free(zData);
        mqtt->Log->Append("Sending message:[").Append(pkg.GetTopic()).Append("]#").Append(pkg.GetPayload()).Append("#").Internal();
        for (auto const &x : mqtt->subscriptions)
        {
            if (pkg.GetTopic() == x.first)
            {
                int32_t e = (x.second.eventId == 0 ? PROTOCOL_RECEIVED_SIGMA_MESSAGE : x.second.eventId);
                esp_err_t res = esp_event_post_to(mqtt->GetEventLoop(), mqtt->GetEventBase(), e, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
                return; // skip other subscription or event
            }
        }
        esp_event_post_to(mqtt->GetEventLoop(), mqtt->GetEventBase(), PROTOCOL_RECEIVED_SIGMA_MESSAGE, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);

        break;
    }
    case MQTT_EVENT_BEFORE_CONNECT:
    {
        mqtt->Log->Append("MQTT_EVENT_BEFORE_CONNECT").Internal();
        break;
    }
    case MQTT_EVENT_DELETED:
    {
        mqtt->Log->Append("MQTT_EVENT_DELETED").Internal();
        break;
    }
    case MQTT_EVENT_ERROR:
    {
        mqtt->Log->Append("MQTT_EVENT_ERROR").Internal();
        esp_mqtt_error_codes_t *error = event->error_handle;
        String errMsg = "MQTT_EVENT_ERROR:" + String(error->error_type) + "/" + String(error->connect_return_code);
        esp_event_post_to(mqtt->GetEventLoop(), mqtt->GetEventBase(), PROTOCOL_ERROR, (void *)errMsg.c_str(), errMsg.length() + 1, portMAX_DELAY);
        break;
    }
    case MQTT_EVENT_SUBSCRIBED:
    {
        //            mqtt->Log->Append("MQTT_EVENT_SUBSCRIBED").Internal();
        break;
    }
    case MQTT_EVENT_UNSUBSCRIBED:
    {
        //          mqtt->Log->Append("MQTT_EVENT_UNSUBSCRIBED").Internal();
        break;
    }
    case MQTT_EVENT_PUBLISHED:
    {
        //        mqtt->Log->Append("MQTT_EVENT_PUBLISHED").Internal();
        break;
    }
    default:
    {
        mqtt->Log->Append("MQTT_EVENT_UNKNOWN").Internal();
        break;
    }
    }
}
