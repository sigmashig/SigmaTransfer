#include "SigmaMQTT.h"
#include <esp_event.h>
#include <WiFi.h>

ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

SigmaMQTT::SigmaMQTT(MqttConfig config)
{
    SetParams(config);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    // mqttClient.onSubscribe(onMqttSubscribe);
    // mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
    // mqttClient.onPublish(onMqttPublish);
    eventMap.clear();
    TimerCallbackFunction_t reconnectByTimer = [](TimerHandle_t xTimer) {
        mqttClient.connect();
    };
    mqttReconnectTimer = xTimerCreate("mqttReconnectTimer", pdMS_TO_TICKS(1000), pdFALSE, NULL, reconnectByTimer);
}

void SigmaMQTT::SetParams(MqttConfig _config)
{
    this->config = _config;
    rootTopic = config.rootTopic;
    bool isIp = SigmaProtocol::IsIP(config.server);
 
    if (isIp)
    {
        IPAddress ip;
        ip.fromString(config.server);
        mqttClient.setServer(ip, config.port);
    }
    else
    {
        mqttClient.setServer(config.server.c_str(), config.port);
    }
    if (config.clientId == "")
    {
        clientId = (String("Sigma_") + String(random(1000))).c_str();
    }
    else
    {
        clientId = config.clientId;
    }
    mqttClient.setClientId(clientId.c_str());
    MLogger->Append("Setting credentials for MQTT:").Append(config.username).Append(":").Append(config.password).Internal();
    mqttClient.setCredentials(config.username.c_str(), config.password.c_str());
    mqttClient.setKeepAlive(config.keepAlive);
}

SigmaMQTT::SigmaMQTT()
{
}

bool SigmaMQTT::BeginSetup()
{
    return true;
}

bool SigmaMQTT::FinalizeSetup()
{
    return true;
}


void SigmaMQTT::Subscribe(TopicSubscription subscriptionTopic)
{
    String topic = config.rootTopic + subscriptionTopic.topic;
    
    //    MLogger->Append("Add to map: ").Append(subscriptionTopic.topic).Internal();
    eventMap[topic] = subscriptionTopic;
    if (mqttClient.connected())
    {
        //      MLogger->Append("Send Subscribe for ").Append(subscriptionTopic.topic).Internal();
        mqttClient.subscribe(topic.c_str(), 0);
    }
    // MLogger->Append("Map size(2): ").Append(eventMap.size()).Internal();
}

void SigmaMQTT::Publish(String topic, String payload)
{
    MLogger->Append("Publish: ").Append(topic).Append(" ").Append(payload).Info();
    MLogger->Append("Connected: ").Append(mqttClient.connected()).Info();
    mqttClient.publish( (config.rootTopic + topic).c_str(), 0, false, payload.c_str());
}

void SigmaMQTT::Unsubscribe(String topic)
{
    String fullTopic = config.rootTopic + topic;
    // MLogger->Append("Unsubscribing from ").Append(topic).Internal();
    mqttClient.unsubscribe(fullTopic.c_str());
    eventMap.erase(fullTopic);
    // MLogger->Append("Map size(3): ").Append(eventMap.size()).Internal();
}
void SigmaMQTT::Connect()
{
 
    MLogger->Append("..Connecting to MQTT").Info();
    mqttClient.connect();
}
void SigmaMQTT::Disconnect()
{
    MLogger->Append("Disconnecting from MQTT").Info();
    // if (mqttReconnectTimer != NULL) {
    //     xTimerStart(mqttReconnectTimer, 0);
    // }
    mqttClient.disconnect();
}
bool SigmaMQTT::IsReady()
{
    PLogger->Append("Checking if MQTT is ready").Internal();
    return mqttClient.connected();
}
void SigmaMQTT::onMqttConnect(bool sessionPresent)
{
    xTimerStop(mqttReconnectTimer, 0);
    MLogger->Append("Posting connected event for ").Append(name).Internal();
    esp_err_t res = PostEvent(PROTOCOL_CONNECTED, (void *)(name.c_str()), name.length() + 1);
    for (auto const &x : eventMap)
    {
        // MLogger->Append("Subscribing to ").Append(x.first).Internal();
        if (x.second.isReSubscribe)
        {
            //   MLogger->Append("Subscribe to ").Append(x.first).Internal();
            mqttClient.subscribe(x.first.c_str(), 0);
        }
    }
    MLogger->Append("Connected to MQTT").Info();
    // for (auto const &x : messages)

    if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        for (auto it = messages.begin(); it != messages.end();)
        {
            if (it->protocolName == name)
            {
                mqttClient.publish(it->topic.c_str(), 0, false, it->payload.c_str());
                it = messages.erase(it);
            }
            else
            {
                it++;
            }
        }
        xSemaphoreGive(queueMutex);
    }
}

void SigmaMQTT::onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    bool isReady = true;
    String sPayload = (len == 0 ? "" : String(payload, len));
    // MLogger->Append("{").Append(len).Append(":").Append(index).Append(":").Append(total).Append("}[").Append(topic).Append("]:").Append(sPayload.substring(0, 100)).Internal();

    String sTopic = String(topic);

    if (len != total)
    { // fragmented message

        topicMsg[sTopic] += sPayload;
        if (index + len == total)
        {
            sPayload = topicMsg[sTopic];
            topicMsg.erase(sTopic);
            isReady = true;
        }
        else
        {
            isReady = false;
        }
    }
    if (isReady)
    {
        SigmaInternalPkg pkg(sTopic, sPayload);
        for (auto const &x : eventMap)
        {
            if (sTopic == x.first)
            {
                int32_t e = (x.second.eventId == 0 ? PROTOCOL_MESSAGE : x.second.eventId);
                esp_err_t res = PostEvent(e, pkg.GetMsg(), strlen(pkg.GetMsg()) + 1);
                return;
            }
        }
        PostEvent(PROTOCOL_MESSAGE, pkg.GetMsg(), strlen(pkg.GetMsg()) + 1);
    }
}

void SigmaMQTT::onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    MLogger->Internal("Disconnected from MQTT");
    PostEvent(PROTOCOL_DISCONNECTED, (void *)(name.c_str()), name.length() + 1);
    if (shouldConnect)
    {
     xTimerStart(mqttReconnectTimer, 0);
    }
}
