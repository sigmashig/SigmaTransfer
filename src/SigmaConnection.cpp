#include "SigmaConnection.h"
#include <WiFi.h>
#include <esp_event.h>
#include <SigmaMQTT.h>
#include <SigmaWSServer.h>
#include <SigmaWSClient.h>

SigmaConnection::SigmaConnection(String name, SigmaLoger *logger, uint priority, uint queueSize, uint stackSize, uint coreId)
{
    this->Log = (logger != nullptr) ? logger : new SigmaLoger(0);
    this->name = name;
    // messages = std::list<Message>();
    // queueMutex = xSemaphoreCreateMutex();
    esp_event_loop_args_t loop_args = {
        .queue_size = (int32_t)queueSize,
        .task_name = name.c_str(),
        .task_priority = priority,
        .task_stack_size = stackSize,
        .task_core_id = (int)coreId};

    esp_err_t espErr = esp_event_loop_create(&loop_args, &eventLoop);
    if (espErr != ESP_OK)
    {
        Log->Printf("Failed to create event loop: %d", espErr).Internal();
        exit(1);
    }
}
void SigmaConnection::SendMessage(String message, int eventId)
{
    esp_event_post_to(GetEventLoop(), GetEventBase(), eventId, (void *)(message.c_str()), message.length() + 1, portMAX_DELAY);
}

SigmaConnection::~SigmaConnection()
{
    esp_event_loop_delete(eventLoop);
}

bool SigmaConnection::IsIP(String URL)
{
    for (int i = 0; i < URL.length(); i++)
    {
        if (URL[i] == '.' || (URL[i] >= '0' && URL[i] <= '9'))
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}

void SigmaConnection::reconnectTask(TimerHandle_t xTimer)
{
    SigmaConnection *conn = (SigmaConnection *)pvTimerGetTimerID(xTimer);
    conn->retryConnectingCount--;
    conn->Log->Append("[reconnectTask]Reconnecting:").Append(conn->retryConnectingCount).Internal();
    conn->Connect();
}

void SigmaConnection::pingTask(TimerHandle_t xTimer)
{
    SigmaConnection *conn = (SigmaConnection *)pvTimerGetTimerID(xTimer);
    if (conn->IsReady())
    {
        conn->sendPing();
    }
}

void SigmaConnection::setReconnectTimer(SigmaConnection *conn)
{
    conn->Log->Append("[setReconnectTimer]:").Append(conn->retryConnectingCount).Internal();
    if (conn->retryConnectingDelay > 0 && conn->retryConnectingCount > 0)
    {
        clearReconnectTimer(conn);
        if (conn->reconnectTimer == nullptr)
        {
            conn->reconnectTimer = xTimerCreate("reconnectTimer", pdMS_TO_TICKS(conn->retryConnectingDelay * 1000), pdFALSE, conn, reconnectTask);
        }
        xTimerReset(conn->reconnectTimer, 0);
    }
}

void SigmaConnection::clearReconnectTimer(SigmaConnection *conn)
{
    if (conn->reconnectTimer != nullptr)
    {
        xTimerStop(conn->reconnectTimer, 0);
        //       conn->reconnectTimer = nullptr;
    }
}

void SigmaConnection::setPingTimer(SigmaConnection *conn)
{
    conn->Log->Append("[setPingTimer]:").Append(conn->pingInterval).Internal();
    if (conn->pingInterval > 0)
    {
        if (conn->pingTimer == nullptr)
        {
            conn->pingTimer = xTimerCreate("pingTimer", pdMS_TO_TICKS(conn->pingInterval * 1000), pdTRUE, conn, pingTask);
        }
        xTimerReset(conn->pingTimer, 0);
    }
}

void SigmaConnection::clearPingTimer(SigmaConnection *conn)
{
    if (conn->pingTimer != nullptr)
    {
        xTimerStop(conn->pingTimer, 0);
    }
}

SigmaConnection *SigmaConnection::Create(SigmaProtocolType type, SigmaConnectionsConfig config, SigmaLoger *logger, uint priority)
{
    switch (type)
    {
    case SIGMA_PROTOCOL_MQTT:
        return new SigmaMQTT(config.mqttConfig, logger, priority);
        break;
    case SIGMA_PROTOCOL_WS_SERVER:
        return new SigmaWsServer(config.wsServerConfig, logger, priority);
        break;
    case SIGMA_PROTOCOL_WS_CLIENT:
        return new SigmaWsClient(config.wsClientConfig, logger, priority);
        break;
    default:
        return nullptr;
    }
    return nullptr;
}

AuthType SigmaConnection::AuthTypeFromString(String typeName)
{
    if (typeName == "AUTH_TYPE_NONE")
    {
        return AUTH_TYPE_NONE;
    }
    else if (typeName == "AUTH_TYPE_URL")
    {
        return AUTH_TYPE_URL;
    }
    else if (typeName == "AUTH_TYPE_BASIC")
    {
        return AUTH_TYPE_BASIC;
    }
    else if (typeName == "AUTH_TYPE_FIRST_MESSAGE")
    {
        return AUTH_TYPE_FIRST_MESSAGE;
    }
    else if (typeName == "AUTH_TYPE_ALL_MESSAGES")
    {
        return AUTH_TYPE_ALL_MESSAGES;
    }
    return AUTH_TYPE_NONE;
}

PingType SigmaConnection::PingTypeFromString(String typeName)
{
    if (typeName == "PING_ONLY_TEXT")
    {
        return PING_ONLY_TEXT;
    }
    else if (typeName == "PING_ONLY_BINARY")
    {
        return PING_ONLY_BINARY;
    }
    return NO_PING;
}

TopicSubscription *SigmaConnection::GetSubscription(String topic)
{
    auto it = subscriptions.find(topic);
    if (it != subscriptions.end())
    {
        return &it->second;
    }
    return nullptr;
}

void SigmaConnection::addSubscription(TopicSubscription subscription)
{
    subscriptions[subscription.topic] = subscription;
}

void SigmaConnection::removeSubscription(String topic)
{
    subscriptions.erase(topic);
}

SigmaProtocolType SigmaConnection::String2Type(String typeName)
{
    if (typeName == "MQTT")
    {
        return SIGMA_PROTOCOL_MQTT;
    }
    else if (typeName == "WS_SERVER")
    {
        return SIGMA_PROTOCOL_WS_SERVER;
    }
    else if (typeName == "WS_CLIENT")
    {
        return SIGMA_PROTOCOL_WS_CLIENT;
    }
    return SIGMA_PROTOCOL_UNKNOWN;
}