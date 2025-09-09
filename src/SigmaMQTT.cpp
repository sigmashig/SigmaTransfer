#include "SigmaMQTT.h"
#include "SigmaInternalPkg.h"
#include <esp_event.h>
#include <WiFi.h>
#include <string.h>
#include <SigmaLoger.h>
#include "SigmaNetworkMgr.h"

ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

SigmaMQTT::SigmaMQTT(MqttConfig config, SigmaLoger *logger, uint priority) : SigmaConnection("SigmaMQTT", config.networkMode, logger, priority)
{
    esp_err_t espErr;
    this->config = config;
    pingInterval = 0; // MQTT doesn't support ping at this moment
    mqttClient = NULL;
    Log->Append("SigmaMQTT init. Network mode:").Append(config.networkMode).Internal();
    Log->Append("Registering network event handler").Internal();
    espErr = SigmaNetworkMgr::RegisterEventHandlers(ESP_EVENT_ANY_ID, networkEventHandler, this);
    if (espErr != ESP_OK)
    {
        Log->Printf("Failed to register NETWORK event handler: %d", espErr).Internal();
    }


    if ((config.networkMode == NETWORK_MODE_LAN && SigmaNetworkMgr::IsLanConnected()) 
    || (config.networkMode == NETWORK_MODE_WAN && SigmaNetworkMgr::IsWanConnected()))
    {
        // init();
        Connect();
    }
}

void SigmaMQTT::init()
{
    Log->Append("MQTT init").Internal();
    esp_mqtt_client_config_t mqtt_cfg;
    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
    esp_err_t err;

    mqtt_cfg.event_loop_handle = getEventLoop();
    Log->Append("MQTT server:").Append(config.server).Append(":").Append(config.port).Debug();
    if (SigmaConnection::IsIP(config.server))
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

    RegisterEventHandlers(PROTOCOL_SEND_SIGMA_MESSAGE, protocolEventHandler, this);
    mqttClient = esp_mqtt_client_init(&mqtt_cfg);
    err = esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY, onMqttEvent, this);
    if (err != ESP_OK)
    {
        Log->Append("Failed to register MQTT event").Error();
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

void SigmaMQTT::protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaMQTT *mqtt = (SigmaMQTT *)arg;
    // mqtt->Log->Append("protocolEventHandler:").Append(event_id).Internal();
    //  SigmaInternalPkg *pkg = (SigmaInternalPkg *)event_data;
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
    Log->Append("MQTT Connect").Internal();
    if (mqttClient == NULL)
    {
        init();
    }
    // Guard: do not start MQTT client unless the desired network is available.
    bool canConnect = true;
    if (config.networkMode == NETWORK_MODE_LAN)
    {
        canConnect = SigmaNetworkMgr::IsLanConnected();
    }
    else if (config.networkMode == NETWORK_MODE_WAN)
    {
        canConnect = SigmaNetworkMgr::IsWanConnected();
    }
    else
    {
        // NONE or unspecified: require at least one network
        canConnect = SigmaNetworkMgr::IsConnected();
    }
    if (!canConnect)
    {
        Log->Append("Network not connected; deferring MQTT start").Warn();
        setReconnectTimer(this);
        return;
    }
    esp_err_t err = esp_mqtt_client_start(mqttClient);
    if (err != ESP_OK)
    {
        Log->Append("MQTT Connect failed:").Append(err).Error();
        isReady = false;
    }
    else
    {
        Log->Append("MQTT Connect success").Internal();
        isReady = true;
    }
}

void SigmaMQTT::Disconnect()
{
    //    Log->Append("Disconnecting from MQTT").Info();
    esp_mqtt_client_stop(mqttClient);
}

void SigmaMQTT::Subscribe(TopicSubscription subscriptionTopic)
{
    subscriptionTopic.topic = config.rootTopic + (config.rootTopic.endsWith("/") ? "" : "/") + subscriptionTopic.topic;

    addSubscription(subscriptionTopic);
    Log->Append("Subscribing to:").Append(subscriptionTopic.topic).Info();
    esp_mqtt_client_subscribe(mqttClient, subscriptionTopic.topic.c_str(), 0);
}

void SigmaMQTT::publish(SigmaInternalPkg *pkg)
{
    String topic = config.rootTopic + (config.rootTopic.endsWith("/") ? "" : "/") + pkg->GetTopic();
    Log->Append("P:[").Append(topic).Append("]#").Append(pkg->GetPayload()).Append("#").Internal();
    // esp_mqtt_client_enqueue(mqttClient, topic.c_str(), pkg->GetPayload().c_str(), pkg->GetPayload().length(), 0, false, true);
    esp_mqtt_client_publish(mqttClient, topic.c_str(), pkg->GetPayload().c_str(), pkg->GetPayload().length(), pkg->GetQos(), pkg->GetRetained());
}

void SigmaMQTT::onMqttEvent(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    SigmaMQTT *mqtt = (SigmaMQTT *)handler_args;
    // mqtt->Log->Append("onMqttEvent:").Append(event_id).Internal();
    esp_mqtt_event_id_t mqtt_event_id = (esp_mqtt_event_id_t)event_id;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    // esp_mqtt_client_handle_t client = (*event)->client;

    switch (mqtt_event_id)
    {
    case MQTT_EVENT_CONNECTED:
    {
        // mqtt->Log->Append("MQTT_EVENT_CONNECTED").Internal();
        mqtt->isReady = true;
        // esp_event_post_to(mqtt->GetEventLoop(), mqtt->GetEventBase(), PROTOCOL_CONNECTED, (void *)mqtt->name.c_str(), mqtt->name.length() + 1, portMAX_DELAY);
        mqtt->PostMessageEvent(mqtt->name, PROTOCOL_CONNECTED);
        for (auto const &x : mqtt->subscriptions)
        {
            if (x.second.isReSubscribe)
            {
                mqtt->Log->Append("Resubscribing to:").Append(x.first).Info();
                esp_mqtt_client_subscribe(mqtt->mqttClient, x.first.c_str(), 0);
            }
        }

        break;
    }
    case MQTT_EVENT_DISCONNECTED:
    {
        mqtt->Log->Append("MQTT  DISCONNECTED").Error();
        mqtt->isReady = false;
        // esp_event_post_to(mqtt->GetEventLoop(), mqtt->GetEventBase(), PROTOCOL_DISCONNECTED, (void *)mqtt->name.c_str(), mqtt->name.length() + 1, portMAX_DELAY);
        mqtt->PostMessageEvent(mqtt->name, PROTOCOL_DISCONNECTED);
        // Try to reconnect later
        mqtt->setReconnectTimer(mqtt);
        break;
    }
    case MQTT_EVENT_DATA:
    {
        // mqtt->Log->Append("MQTT_EVENT_DATA").Internal();
        if (event->topic_len == 0)
        {
            mqtt->Log->Append("Topic length is zero. Ignore").Warn();
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
        mqtt->Log->Append("[").Append(pkg.GetTopic()).Append("]#").Append(pkg.GetPayload()).Append("#").Internal();
        for (auto const &x : mqtt->subscriptions)
        {
            if (pkg.GetTopic() == x.first)
            {
                int32_t e = (x.second.eventId == 0 ? PROTOCOL_RECEIVED_SIGMA_MESSAGE : x.second.eventId);
                // esp_err_t res = esp_event_post_to(mqtt->GetEventLoop(), mqtt->GetEventBase(), e, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
                mqtt->PostMessageEvent(pkg.GetPkgString(), e);
                return; // skip other subscription or event
            }
        }
        // esp_event_post_to(mqtt->GetEventLoop(), mqtt->GetEventBase(), PROTOCOL_RECEIVED_SIGMA_MESSAGE, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
        mqtt->PostMessageEvent(pkg.GetPkgString(), PROTOCOL_RECEIVED_SIGMA_MESSAGE);

        break;
    }
    case MQTT_EVENT_BEFORE_CONNECT:
    {
        // mqtt->Log->Append("MQTT_EVENT_BEFORE_CONNECT").Internal();
        break;
    }
    case MQTT_EVENT_DELETED:
    {
        // mqtt->Log->Append("MQTT_EVENT_DELETED").Internal();
        break;
    }
    case MQTT_EVENT_ERROR:
    {
        esp_mqtt_error_codes_t *error = event->error_handle;
        String errMsg = "MQTT_EVENT_ERROR:" + String(error->error_type) + "/" + String(error->connect_return_code);
        mqtt->Log->Append("MQTT_EVENT_ERROR:").Append(errMsg).Error();
        // esp_event_post_to(mqtt->GetEventLoop(), mqtt->GetEventBase(), PROTOCOL_ERROR, (void *)errMsg.c_str(), errMsg.length() + 1, portMAX_DELAY);
        mqtt->PostMessageEvent(errMsg, PROTOCOL_ERROR);
        mqtt->setReconnectTimer(mqtt);
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
        mqtt->Log->Append("MQTT_EVENT_UNKNOWN:").Append(mqtt_event_id).Error();
        break;
    }
    }
}

void SigmaMQTT::networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaMQTT *mqtt = (SigmaMQTT *)arg;
    mqtt->connectionHandler(event_id, event_data);
}
