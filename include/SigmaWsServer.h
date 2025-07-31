#ifndef SIGMAWSSERVER_H
#define SIGMAWSSERVER_H

#pragma once
#include <Arduino.h>
#include <SigmaLoger.h>
#include <SigmaInternalPkg.h>
#include <map>
#include <esp_event.h>
#include "SigmaConnection.h"
#include "SigmaTransferDefs.h"
#include <ArduinoJson.h>
#include <esp_http_server.h>

typedef struct FullAddress
{
    IPAddress ip;
    uint port;
} FullAddress;

typedef struct TransferPkg
{
    httpd_handle_t server;
    int32_t socketNumber;
    byte type;
    String message;
    void *buffer;
    size_t len;
} TransferPkg;

typedef struct ClientAuth
{
    String clientId = ""; // the string ID as provided by client
    FullAddress fullAddress = {IPAddress(0, 0, 0, 0), 0};
    int32_t socketNumber = 0;
    bool isAuth = false;
    PingType pingType = PING_NO;
    int pingRetryCount = 3;
    // ClientAuth() : clientId(""), fullAddress{IPAddress(0, 0, 0, 0), 0}, isAuth(false), pingType(NO_PING), pingRetryCount(3) {}
} ClientAuth;

class SigmaWsServer : public SigmaConnection
{
public:
    // Authentication:
    // URL: add ?clientId=clientId&authKey=authKey&pingType=NO_PING
    // Basic: ...will be added later...
    // First Message: {"type":"auth","authKey":"secret-api-key-12345","clientId":"RM_C_Green01"}
    // All Messages: JSON ONLY {"type":"auth","authKey":"secret-api-key-12345","clientId":"RM_C_Green01", "data":"{your data as json}"}
    // The allowable clients should be added before the connection is established
    // The clientId is the string ID as provided by client
    // The authKey is the key to authenticate the client
    // The pingType is the type of ping to send to the client
    // The pingType is optional. If not provided, it will be set to PING_ONLY_TEXT.
    //            available values: NO_PING, PING_ONLY_TEXT, PING_ONLY_BINARY
    // The data is the data to send to the client

    SigmaWsServer(WSServerConfig config, SigmaLoger *logger, int priority = 5);
    ~SigmaWsServer();

    void AddAllowableClient(String clientId, String authKey)
    {
        AllowableClient client;
        client.clientId = clientId;
        client.authKey = authKey;
        allowableClients[clientId] = client;
        Serial.printf("AddAllowableClient: clientId: %s, authKey: %s, pingType: %d\n", client.clientId.c_str(), client.authKey.c_str());
    }

    void AddAllowableClient(AllowableClient client)
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

private:
    httpd_handle_t server = NULL;
    WSServerConfig config;
    std::map<String, AllowableClient> allowableClients; // clientId -> AllowableClients
    std::map<int32_t, ClientAuth> clients;              // socketNumber -> ClientAuth
    inline static QueueHandle_t xQueue;
    bool shouldConnect = true;
    httpd_config_t serverConfig = HTTPD_DEFAULT_CONFIG();
    inline static SemaphoreHandle_t requestSemaphore;
    inline static StaticSemaphore_t requestBuffer;

    void Connect();
    void Disconnect();
    void Close();
    void sendPing();

    static esp_err_t onWsEvent(httpd_req_t *req);
    static void sendWSFrame(void *arg);
    static void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    bool isClientAvailable(String clientId, String authKey);
    bool sendMessageToClient(int32_t socketNumber, String message);
    bool sendPongToClient(int32_t socketNumber, String message);
    bool sendPingToClient(int32_t socketNumber, String message);
    static void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void processData(void *arg);
    bool sendPingToClient(ClientAuth &auth, String payload, bool isRemoveClient = true);
    bool sendPongToClient(ClientAuth &auth, String payload);
    bool sendMessageToClient(ClientAuth &auth, String message);
    void handleTextPackage(uint8_t *payload, size_t len, int32_t socketNumber, httpd_req_t *req);
    bool clientAuthRequest(httpd_req_t *req, String &request, ClientAuth *auth, String &payload);
    static FullAddress getClientFullAddress(int32_t socketNumber);
    static FullAddress getClientFullAddress(httpd_req_t *req);

    // uint64_t getClientNumber(FullAddress fullAddress);

    bool removeClient(int32_t socketNumber);

    int32_t isClientConnected(String clientId);

    // AsyncWebServer *server;
    // AsyncWebSocket *ws;

    // void clearReconnect(){/*nothing todo */};
    // bool sendMessageToClient(String clientId, String message);
    // bool sendMessageToClient(ClientAuth auth, String message);
    // bool sendMessageToClient(String clientId, byte *data, size_t size);
    // bool sendPingToClient(String clientId, String payload);
    // bool sendPingToClient(ClientAuth auth, String payload);
    // bool sendPongToClient(String clientId, String payload);
    // bool sendPongToClient(ClientAuth auth, String payload);
    // void setReady(bool ready) { isReady = ready; };

    // static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
    // static void handleWebSocketMessage(SigmaWsServer *server, SigmaWsServerData data);
    // bool clientAuthRequest(String payload, String &clientId);
    // bool isClientAvailable(String clientId, String authKey);

    // bool isClientLimitReached(SigmaWsServer *server, AsyncWebSocketClient *client);

    // bool isConnectionLimitReached(String clientId, SigmaWsServer *server, AsyncWebSocketClient *client);

    // bool sendMessageToClient(int32_t clientNumber, String message);
    // bool shouldConnect = true;
    // void Connect();
    // void Disconnect();
    // void Close();
    // void sendPing();
};

#endif
