#include "SigmaMQTT.h"
#include <esp_event.h>
#include <WiFi.h>

ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

SigmaMQTT::SigmaMQTT(String name, SigmaLoger *logger, MqttConfig config, uint priority) : SigmaProtocol(name, logger, priority)
{
    this->config = config;
    if (SigmaProtocol::IsIP(config.server))
    {
        IPAddress ip;
        ip.fromString(config.server);
        mqttClient.setServer(ip, config.port);
    }
    else
    {
        mqttClient.setServer(config.server.c_str(), config.port);
    }
    Log->Append("Setting client ID for MQTT:").Append(config.clientId).Internal();
    strcpy(clientid, config.clientId.c_str());
    mqttClient.setClientId(clientid);
    strcpy(username, config.username.c_str());
    strcpy(password, config.password.c_str());
    Log->Append("Setting credentials for MQTT:").Append(username).Append(":").Append(password).Internal();
    mqttClient.setCredentials(username, password);
    mqttClient.setKeepAlive(config.keepAlive);

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    // mqttClient.onSubscribe(onMqttSubscribe);
    // mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
    // mqttClient.onPublish(onMqttPublish);
    mqttReconnectTimer = xTimerCreate("mqttReconnectTimer", pdMS_TO_TICKS(1000), pdFALSE, this, reconnectByTimer);

    esp_event_loop_args_t loop_args = {
        .queue_size = (int32_t)10,
        .task_name = name.c_str(),
        .task_priority = priority + 10,
        .task_stack_size = 4096 /*,
         .task_core_id = 0*/
    };

    esp_err_t espErr = esp_event_loop_create(&loop_args, &internalLoop);
    if (espErr != ESP_OK)
    {
        Log->Printf("Failed to create event loop: %d", espErr).Internal();
        exit(1);
    }

    // mqttQueue = xQueueCreate(10, sizeof(MqttQueueTask));
    xTaskCreate(processMqttQueue, "ProcessMqttQueue", 4096, this, 5, NULL);
    queueMutex = xSemaphoreCreateMutex();
    esp_event_handler_register_with(SigmaAsyncNetwork::GetEventLoop(), SIGMAASYNCNETWORK_EVENT, ESP_EVENT_ANY_ID, networkEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, protocolEventHandler, this);
}

SigmaMQTT::~SigmaMQTT()
{
}

void SigmaMQTT::reconnectByTimer(TimerHandle_t xTimer)
{
    esp_event_post_to(internalLoop, internalBase, PROTOCOL_SEND_SIGMA_MESSAGE, (void *)(&xTimer), sizeof(xTimer), portMAX_DELAY);
}

void SigmaMQTT::onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    esp_event_post_to(internalLoop, internalBase, PROTOCOL_SEND_SIGMA_MESSAGE, (void *)(&reason), sizeof(reason), portMAX_DELAY);
}

void SigmaMQTT::onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    Serial.println("************************************************");
    Serial.printf("onMqttMessage2:len=[%d]#index=[%d]#total=[%d]\n", len, index, total);
    MqttQueueTask task;

    if (len == 0)
    {
        task.payload = "";
    }
    else
    {
        char *p = (char *)malloc(len + 1);
        memcpy(p, payload, len);
        p[len] = '\0';
        task.payload = String(p);
        free(p);
    }
    // Serial.printf("onMqttMessage:[%s]#%s#\n", topic, payload);
    task.type = MqttQueueTask::Q_MQTTMESSAGE;
    task.properties = properties;
    task.topic = topic;
    // task.payload = payload;
    task.len = len;
    task.index = index;
    task.total = total;
    Serial.printf("onMqttMessage3:type=[%d]#topic=[%s]#payload=[%s]#\n", task.type, task.topic.c_str(), task.payload.c_str());
    xQueueSend(mqttQueue, &task, portMAX_DELAY);
}

void SigmaMQTT::onTimer(TimerHandle_t xTimer)
{
    mqttClient.connect();
}

void SigmaMQTT::processMqttQueue(void *arg)
{
    SigmaMQTT *mqtt = (SigmaMQTT *)arg;
    MqttQueueTask task;
    while (true)
    {
        if (xQueueReceive(mqtt->mqttQueue, &task, portMAX_DELAY) == pdPASS)
        {
            mqtt->Log->Append("Processing MQTT queue:").Append(task.type).Internal();
            switch (task.type)
            {
            case MqttQueueTask::Q_MQTTCONNECT:
                mqtt->onConnect(task.sessionPresent);
                break;
            case MqttQueueTask::Q_MQTTDISCONNECT:
                mqtt->onDisconnect(task.reason);
                break;
            case MqttQueueTask::Q_MQTTMESSAGE:
                mqtt->onMessage(task.topic.c_str(), task.payload.c_str(), task.properties, task.len, task.index, task.total);
                break;
            case MqttQueueTask::Q_TIMER:
                mqtt->reconnectByTimer(task.timer);
                break;
            }
            Serial.printf("Processed MQTT queue: %d\n", task.type);
        }
    }
}

void SigmaMQTT::networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaMQTT *mqtt = (SigmaMQTT *)arg;
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

void SigmaMQTT::Subscribe(TopicSubscription subscriptionTopic)
{
    Log->Append("POINT 1").Internal();
    subscriptionTopic.topic = config.rootTopic + subscriptionTopic.topic;

    addSubscription(subscriptionTopic);
    Log->Append("Subscribing to:").Append(subscriptionTopic.topic).Internal();
    if (mqttClient.connected())
    {
        Log->Append("Connected  to MQTT server").Internal();
        mqttClient.subscribe(subscriptionTopic.topic.c_str(), 0);
    }
}

void SigmaMQTT::publish(SigmaInternalPkg *pkg)
{
    if (mqttClient.connected())
    {
        Log->Append("Publish:[").Append(pkg->GetTopic()).Append("]#").Append(pkg->GetPayload()).Append("#").Info();
        mqttClient.publish((config.rootTopic + pkg->GetTopic()).c_str(), 0, false, pkg->GetPayload().c_str());
    }
    else
    {
        Log->Append("Not connected to MQTT server. Adding to queue").Error();
        if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            messages.push(*pkg);
            xSemaphoreGive(queueMutex);
        }
    }
}

void SigmaMQTT::Unsubscribe(String topic)
{
    String fullTopic = config.rootTopic + topic;
    mqttClient.unsubscribe(fullTopic.c_str());
    removeSubscription(fullTopic);
}

void SigmaMQTT::Connect()
{
    // Log->Append("..Connecting to MQTT..").Info();
    mqttClient.connect();
}

void SigmaMQTT::Disconnect()
{
    Log->Append("Disconnecting from MQTT").Info();
    mqttClient.disconnect();
}

bool SigmaMQTT::IsReady()
{
    return mqttClient.connected();
}

void SigmaMQTT::onConnect(bool sessionPresent)
{
    xTimerStop(mqttReconnectTimer, 0);

    esp_err_t res = esp_event_post_to(GetEventLoop(), GetEventBase(), PROTOCOL_CONNECTED, (void *)(name.c_str()), name.length() + 1, portMAX_DELAY);
    if (res != ESP_OK)
    {
        Log->Append("Failed to post connected event").Error();
    }

    for (auto const &x : subscriptions)
    {
        if (x.second.isReSubscribe)
        {
            mqttClient.subscribe(x.first.c_str(), 0);
        }
    }
    Log->Append("Connected to MQTT").Info();
    // for (auto const &x : messages)
    if (xSemaphoreTake(queueMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        while (!messages.empty())
        {
            Log->Append("...Sending message to MQTT...").Internal();
            SigmaInternalPkg pkg = messages.front();
            messages.pop();
            mqttClient.publish(pkg.GetTopic().c_str(), 0, false, pkg.GetPayload().c_str());
        }
        xSemaphoreGive(queueMutex);
    }
    Log->Append("Sent messages to MQTT").Internal();
}

void SigmaMQTT::onMqttConnect(bool sessionPresent)
{
    Serial.println("onMqttConnect");
    MqttQueueTask task;
    task.type = MqttQueueTask::Q_MQTTCONNECT;
    task.sessionPresent = sessionPresent;
    xQueueSend(mqttQueue, &task, portMAX_DELAY);
}

void SigmaMQTT::onMessage(const char *topic, const char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
    bool isReady = true;
    String sPayload = (len == 0 ? "" : String(payload, len));

    String sTopic = String(topic);
    Log->Append("Received message:[").Append(sTopic).Append("]#").Append(sPayload).Append("#").Internal();

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
        Log->Append("Sending message:[").Append(pkg.GetTopic()).Append("]#").Append(pkg.GetPayload()).Append("#").Internal();
        for (auto const &x : subscriptions)
        {
            if (sTopic == x.first)
            {
                int32_t e = (x.second.eventId == 0 ? PROTOCOL_RECEIVED_SIGMA_MESSAGE : x.second.eventId);
                esp_err_t res = esp_event_post_to(GetEventLoop(), GetEventBase(), e, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
                return; // skip other subscription or event
            }
        }
        esp_event_post_to(GetEventLoop(), GetEventBase(), PROTOCOL_RECEIVED_SIGMA_MESSAGE, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
    }
}

void SigmaMQTT::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
    Log->Append("Disconnected from MQTT").Info();
    esp_err_t res = esp_event_post_to(GetEventLoop(), GetEventBase(), PROTOCOL_DISCONNECTED, (void *)(name.c_str()), name.length() + 1, portMAX_DELAY);
    if (shouldConnect)
    {
        xTimerStart(mqttReconnectTimer, 0);
    }
}
