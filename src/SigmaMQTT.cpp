#include "SigmaMQTT.h"
#include <esp_event.h>
#include <WiFi.h>

//ESP_EVENT_DEFINE_BASE(SIGMAMQTT_EVENT);
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
}

void SigmaMQTT::SetParams(MqttConfig config)
{
    this->config = config;
    bool isIp = true;
    for (int i = 0; i < config.server.length(); i++)
    {
        if (config.server[i] == '.' || (config.server[i] >= '0' && config.server[i] <= '9'))
        {
            continue;
        }
        else
        {
            isIp = false;
            break;
        }
    }

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
    if (config.clientId == "") {
        clientId = (String("Sigma_") + String(random(1000))).c_str();
    } else {
        clientId = config.clientId;
    }
    MLogger->Append("ClientId: ").Append(config.clientId).Internal();
    mqttClient.setClientId(clientId.c_str());
    mqttClient.setCredentials(config.username.c_str(), config.password.c_str());

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

void SigmaMQTT::ConnectToMqtt()
{
    MLogger->Internal("------------------------------Connecting to MQTT...");
    mqttClient.connect();
}


void SigmaMQTT::Subscribe(TopicSubscription subscriptionTopic, String rootTopic)
{
    if (rootTopic != "")
    {
        subscriptionTopic.topic = rootTopic + subscriptionTopic.topic;
    }
    //    MLogger->Append("Add to map: ").Append(subscriptionTopic.topic).Internal();
    eventMap[subscriptionTopic.topic] = subscriptionTopic;
    if (mqttClient.connected())
    {
        //      MLogger->Append("Send Subscribe for ").Append(subscriptionTopic.topic).Internal();
        mqttClient.subscribe(subscriptionTopic.topic.c_str(), 0);
    }
    // MLogger->Append("Map size(2): ").Append(eventMap.size()).Internal();
}

void SigmaMQTT::Publish(String topic, String payload)
{
    MLogger->Append("Publish: ").Append(topic).Append(" ").Append(payload).Info();
    MLogger->Append("Connected: ").Append(mqttClient.connected()).Info();
    mqttClient.publish(topic.c_str(), 0, false, payload.c_str());
}

void SigmaMQTT::Unsubscribe(String topic, String rootTopic)
{
    if (rootTopic != "")
    {
        topic = rootTopic + topic;
    }
    // MLogger->Append("Unsubscribing from ").Append(topic).Internal();
    mqttClient.unsubscribe(topic.c_str());
    eventMap.erase(topic);
    // MLogger->Append("Map size(3): ").Append(eventMap.size()).Internal();
}
void SigmaMQTT::Connect()
{
  //  if (mqttReconnectTimer != NULL) {
  //      xTimerStop(mqttReconnectTimer, 0);
  //  }
  
    MLogger->Append("..Connecting to MQTT").Info();
    mqttClient.connect();
}
void SigmaMQTT::Disconnect()
{
    MLogger->Append("Disconnecting from MQTT").Info();
    //if (mqttReconnectTimer != NULL) {
    //    xTimerStart(mqttReconnectTimer, 0);
    //}
    mqttClient.disconnect();
}
bool SigmaMQTT::IsConnected()
{
    return mqttClient.connected();
}
void SigmaMQTT::onMqttConnect(bool sessionPresent)
{
    MLogger->Append("Posting connected event for ").Append(name).Internal();
    esp_err_t res = esp_event_post(SIGMATRANSFER_EVENT, PROTOCOL_CONNECTED, (void*) (name.c_str()), name.length()+1, portMAX_DELAY);
    for (auto const &x : eventMap)
    {
        // MLogger->Append("Subscribing to ").Append(x.first).Internal();
        if (x.second.isReSubscribe)
        {
            //   MLogger->Append("Subscribe to ").Append(x.first).Internal();
            mqttClient.subscribe(x.first.c_str(), 0);
        }
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
                esp_err_t res = esp_event_post(SIGMATRANSFER_EVENT, e, pkg.GetMsg(), strlen(pkg.GetMsg()) + 1, portMAX_DELAY);
                return;
            }
        }
        esp_event_post(SIGMATRANSFER_EVENT, PROTOCOL_MESSAGE, (void *)(pkg.GetMsg()), len, portMAX_DELAY);
    }
}

void SigmaMQTT::onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    MLogger->Internal("Disconnected from MQTT");
    esp_err_t res = esp_event_post(SIGMATRANSFER_EVENT, PROTOCOL_DISCONNECTED, NULL, 0, portMAX_DELAY);
    //if (WiFi.isConnected())
    //{
    //    xTimerStart(mqttReconnectTimer, 0);
    //}
}
