#ifndef SIGMAWSSERVER_H
#define SIGMAWSSERVER_H

#pragma once
#include <Arduino.h>
#include <SigmaLoger.h>
#include <SigmaInternalPkg.h>
#include <map>
#include <esp_event.h>
#include "SigmaProtocol.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SigmaTransferDefs.h"

typedef struct
{
    String clientId;
    String authKey;
} AllowableClients;

typedef struct
{
    String clientId;
    // String authKey;
    int32_t clientIdInt;
    bool isAuth = false;
    AsyncWebSocketClient *wsClient;
} ClientAuth;

typedef struct
{
    AsyncWebSocket *wsServer;
    AsyncWebSocketClient *wsClient;
    AwsFrameInfo *frameInfo;
    uint8_t *data;
    AwsEventType type;
    size_t len;
} SigmaWsServerData;

class SigmaWsServer : public SigmaProtocol
{
public:
    SigmaWsServer(String name, SigmaLoger *logger, WSServerConfig config, int priority = 5);
    ~SigmaWsServer();
    // void Subscribe(TopicSubscription subscriptionTopic) {};
    // void Unsubscribe(String topic) {};

    void AddAllowableClient(String clientId, String authKey)
    {
        AllowableClients client;
        client.clientId = clientId;
        client.authKey = authKey;
        allowableClients[clientId] = client;
    }
    void AddAllowableClient(AllowableClients client)
    {
        allowableClients[client.clientId] = client;
    }
    void RemoveAllowableClient(String clientId)
    {
        allowableClients.erase(clientId);
    }
    const ClientAuth *GetClientAuth(String clientId)
    {
        for (auto const &client : clients)
        {
            if (client.second.clientId == clientId)
            {
                return &client.second;
            }
        }
        return nullptr;
    }
    bool sendMessageToClient(String clientId, String message);
    bool sendMessageToClient(String clientId, byte *data, size_t size);
    bool sendPingToClient(String clientId, String payload);
    bool sendPongToClient(String clientId, String payload);
    void setReady(bool ready) { isReady = ready; };

private:
    // SigmaLoger *Log = new SigmaLoger(512);
    WSServerConfig config;

    std::map<String, TopicSubscription> eventMap;
    // inline static bool shouldConnect = true;
    AsyncWebServer *server;
    AsyncWebSocket *ws;
    std::map<String, AllowableClients> allowableClients;

    static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    static void handleWebSocketMessage(SigmaWsServer *server, SigmaWsServerData data);
    static void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    inline static std::map<int32_t, ClientAuth> clients;
    bool clientAuthRequest(String payload, String &clientId);
    bool isClientAvailable(String clientId, String authKey);

    bool isClientLimitReached(SigmaWsServer *server, AsyncWebSocketClient *client);

    bool isConnectionLimitReached(String clientId, SigmaWsServer *server, AsyncWebSocketClient *client);

    static void processData(void *arg);
    inline static QueueHandle_t xQueue;
    static void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    bool shouldConnect = true;
    void Connect();
    void Disconnect();
    // bool IsNetworkRequired() { return true; };
    void Close();
};

#endif
