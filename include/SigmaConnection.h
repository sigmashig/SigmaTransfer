#ifndef SIGMACONNECTION_H
#define SIGMACONNECTION_H
#pragma once

#include <SigmaLoger.h>
#include <map>
#include <list>
#include <esp_event.h>
// #include <SigmaAsyncNetwork.h>
#include "SigmaTransferDefs.h"

typedef struct
{
    String protocolName;
    String topic;
    String payload;
} Message;

class SigmaConnection
{
public:
    SigmaConnection(String name, NetworkMode networkMode, SigmaLoger *logger = nullptr, uint priority = 5, uint queueSize = 100, uint stackSize = 4096, uint coreId = 1);
    virtual void PostMessageEvent(String message, int eventId);
    ~SigmaConnection();
    virtual void Subscribe(TopicSubscription subscriptionTopic)
    {
        addSubscription(subscriptionTopic);
    };

    virtual void Subscribe(String topic)
    {
        TopicSubscription pkg;
        pkg.topic = rootTopic + topic;
        Subscribe(pkg);
    };
    virtual void Unsubscribe(String topic)
    {
        removeSubscription(topic);
    };
    virtual void Unsubscribe(TopicSubscription topic) { Unsubscribe(topic.topic); };

    // virtual esp_event_base_t GetEventBase() { return eventBase; };
    virtual String GetName() { return name; };
    virtual bool IsReady() { return isReady; };
    static SigmaConnection *Create(String TypeName, SigmaConnectionsConfig config, SigmaLoger *logger = nullptr, uint priority = 5) { return Create(String2Type(TypeName), config, logger, priority); };
    static SigmaConnection *Create(SigmaProtocolType type, SigmaConnectionsConfig config, SigmaLoger *logger = nullptr, uint priority = 5);
    static AuthType AuthTypeFromString(String typeName);
    static PingType PingTypeFromString(String typeName);
    esp_err_t RegisterEventHandlers(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);

protected:
    esp_event_base_t eventBase = "SigmaConnection";
    // bool isNetworkRequired = false;
    String rootTopic = "/";
    bool isReady = false;
    std::map<String, TopicSubscription> subscriptions;
    String clientId;
    String name;
    esp_event_loop_handle_t eventLoop;
    SigmaLoger *Log;
    TimerHandle_t reconnectTimer = nullptr;
    TimerHandle_t pingTimer = nullptr;
    int pingInterval = 60000; // 60 seconds
    // int pingRetryCount = 3;   // disconnect after 3 pings when applicable
    int retryConnectingCount = 3;
    uint retryConnectingDelay = 1000;
    NetworkMode networkMode = NETWORK_MODE_NONE;

    virtual void Disconnect() = 0;
    virtual void Close() = 0;
    virtual void Connect() = 0;
    virtual void setReady(bool ready) { isReady = ready; };
    virtual void sendPing() = 0;
    // virtual void clearReconnect() = 0;
    esp_event_loop_handle_t getEventLoop() { return eventLoop; };
    void setRootTopic(String rootTopic) { this->rootTopic = rootTopic; };
    TopicSubscription *GetSubscription(String topic);
    void addSubscription(TopicSubscription subscription);
    void removeSubscription(String topic);
    static SigmaProtocolType String2Type(String typeName);
    static bool IsIP(String URL);
    static void reconnectTask(TimerHandle_t xTimer);
    void setReconnectTimer(SigmaConnection *conn);
    static void pingTask(TimerHandle_t xTimer);
    void clearReconnectTimer(SigmaConnection *conn);

    void setPingTimer(SigmaConnection *conn);
    void clearPingTimer(SigmaConnection *conn);
    static void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

private:
};

#endif