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

typedef struct
{
    uint16_t port = 80;
    String rootPath = "/";
    // String apiKey = "";
    byte authType = AUTH_TYPE_NONE;

} WSServerConfig;

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
    SigmaWsServer(String name, WSServerConfig config);
    ~SigmaWsServer();
    void Subscribe(TopicSubscription subscriptionTopic) {};
    void Unsubscribe(String topic) {};

    void Connect();
    void Disconnect();
    bool IsNetworkRequired() { return true; };
    void Close();

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
    SigmaLoger *Log = new SigmaLoger(512);
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
    bool clientAuthRequest(String payload);
    bool isClientAvailable(String clientId, String authKey);

    static void processData(void *arg);
    inline static QueueHandle_t xQueue;
    static void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    bool shouldConnect = true;

    // inline static AsyncClient wsClient;
    // static void onConnect(void *arg, AsyncClient *c);
    // static void onDisconnect(void *arg, AsyncClient *c);
    // static void onData(void *arg, AsyncClient *c, void *data, size_t len);
    // static void onError(void *arg, AsyncClient *c, int8_t error);
    // static void onTimeout(void *arg, AsyncClient *c, uint32_t time);
    // bool sendWebSocketFrame(const byte *payload, size_t payloadLen, byte opcode, bool isAuth = false);
    // void setReady(bool ready);
    // static void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    // static void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    // bool sendWebSocketTextFrame(const String &payload, bool isAuth = false);
    // bool sendWebSocketBinaryFrame(const byte *data, size_t size);
    // bool sendWebSocketPingFrame(const String &payload);
    // bool sendWebSocketPongFrame(const String &payload);
    // bool sendWebSocketCloseFrame();
};

#endif
