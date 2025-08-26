#include "SigmaConnection.h"
#include <WiFi.h>
#include <esp_event.h>
#include <SigmaMQTT.h>
#include <SigmaWSServer.h>
#include <SigmaWSClient.h>
#include <SigmaNetworkMgr.h>

SigmaConnection::SigmaConnection(String name, NetworkMode networkMode, SigmaLoger *logger, uint priority, uint queueSize, uint stackSize, uint coreId)
{
    this->Log = (logger != nullptr) ? logger : new SigmaLoger(0);
    this->name = name;
    this->networkMode = networkMode;
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
    }
    Log->Append("Registering network event handler").Internal();
    espErr = SigmaNetworkMgr::RegisterEventHandlers(ESP_EVENT_ANY_ID, networkEventHandler, this);
    if (espErr != ESP_OK)
    {
        Log->Printf("Failed to register NETWORK event handler: %d", espErr).Internal();
    }
    Log->Append("Network event handler registered").Internal();
}
void SigmaConnection::PostMessageEvent(String message, int eventId)
{
    esp_event_post_to(eventLoop, eventBase, eventId, (void *)(message.c_str()), message.length() + 1, portMAX_DELAY);
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
        conn->Log->Append("[pingTask]Retrying:").Append(conn->retryConnectingCount).Internal();
        //        conn->Log->Append("[pingTask]Sending ping").Internal();
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
    if (conn->pingInterval > 0)
    {
        if (conn->pingTimer == nullptr)
        {
            conn->pingTimer = xTimerCreate("pingTimer", pdMS_TO_TICKS(conn->pingInterval * 1000), pdTRUE, conn, pingTask);
        }
        if (conn->pingTimer != nullptr)
        {
            conn->Log->Append("[setPingTimer]Resetting ping timer").Internal();
            xTimerReset(conn->pingTimer, 0);
        }
        else
        {
            conn->Log->Append("[setPingTimer]Failed to create ping timer").Internal();
        }
    }
    else
    {
        conn->Log->Append("[setPingTimer]Ping interval is 0").Internal();
    }
}

void SigmaConnection::clearPingTimer(SigmaConnection *conn)
{
    if (conn->pingTimer != nullptr)
    {
        xTimerStop(conn->pingTimer, 0);
    }
}

void SigmaConnection::networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaConnection *conn = (SigmaConnection *)arg;
    // conn->Log->Append("[networkEventHandler]event_id=").Append(event_id).Internal();

    switch (event_id)
    {
    case NETWORK_WAN_CONNECTED:
    {
        if (conn->networkMode == NETWORK_MODE_WAN)
        {
            // conn->Log->Append("[networkEventHandler]WAN connected").Internal();
            conn->Connect();
        }
        break;
    }
    case NETWORK_LAN_CONNECTED:
    {
        if (conn->networkMode == NETWORK_MODE_LAN)
        {
            // conn->Log->Append("[networkEventHandler]LAN connected").Internal();
            conn->Connect();
        }
        break;
    }
    case NETWORK_WAN_DISCONNECTED:
    {
        // conn->Log->Append("[networkEventHandler]WAN disconnected").Internal();
        conn->Disconnect();
        break;
    }
    case NETWORK_LAN_DISCONNECTED:
    {
        conn->Log->Append("[networkEventHandler]LAN disconnected").Internal();
        conn->Disconnect();
        break;
    }
    }
}

SigmaConnection *SigmaConnection::Create(SigmaProtocolType type, SigmaConnectionsConfig config, SigmaLoger *logger, uint priority)
{
    switch (type)
    {
    case SIGMA_PROTOCOL_MQTT:
        logger->Append("Creating SigmaMQTT").Internal();
        return new SigmaMQTT(config.mqttConfig, logger, priority);
        break;
    case SIGMA_PROTOCOL_WS_SERVER:
        logger->Append("Creating SigmaWsServer").Internal();
        return new SigmaWsServer(config.wsServerConfig, logger, priority);
        break;
    case SIGMA_PROTOCOL_WS_CLIENT:
        logger->Append("Creating SigmaWsClient").Internal();
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
    if (typeName == "PING_TEXT")
    {
        return PING_TEXT;
    }
    else if (typeName == "PING_BINARY")
    {
        return PING_BINARY;
    }
    return PING_NO;
}

esp_err_t SigmaConnection::RegisterEventHandlers(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg)
{
    return esp_event_handler_register_with(eventLoop, eventBase, event_id, event_handler, event_handler_arg);
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