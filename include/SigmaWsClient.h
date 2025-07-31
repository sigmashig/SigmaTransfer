#ifndef SIGMAWSCLIENT_H
#define SIGMAWSCLIENT_H

#pragma once
#include <Arduino.h>
#include <SigmaLoger.h>
#include <SigmaInternalPkg.h>
#include <map>
#include <esp_event.h>
#include "SigmaConnection.h"
#include "SigmaTransferDefs.h"
#include <esp_websocket_client.h>
typedef enum
{
    WS_TEXT = 0x1,
    WS_BINARY = 0x2,
    WS_DISCONNECT = 0x8,
    WS_PING = 0x9,
    WS_PONG = 0xA
} WebSocketOpcode;

class SigmaWsClient : public SigmaConnection
{
public:
    // Authentication:
    // URL: add ?clientId=clientId&authKey=authKey&pingType=NO_PING
    // Basic: ...will be added later...
    // First Message: {"type":"auth","authKey":"secret-api-key-12345","clientId":"RM_C_Green01","pingType":"NO_PING"}
    // All Messages: JSON ONLY {"type":"auth","authKey":"secret-api-key-12345","clientId":"RM_C_Green01","pingType":"NO_PING", "data":"{your data as json}"}
    //
    // This client automatically sends pong (binary) to the server by ping request
    // "pingType" is optional. If not provided, it will be set to NO_PING.
    //            available values: NO_PING, PING_ONLY_TEXT, PING_ONLY_BINARY
    SigmaWsClient(WSClientConfig config, SigmaLoger *logger = nullptr, uint priority = 5);

private:
    WSClientConfig config;
    //bool isReady = false;
    // inline static AsyncClient wsClient;
    bool shouldConnect = true;
    int pingRetryCount = 0;

    esp_websocket_client_handle_t wsClient = NULL;
    esp_websocket_client_config_t wsClientConfig;

    void
        Connect();
    void Disconnect();
    void sendPing();
    void Close()
    {
        shouldConnect = false;
        Disconnect();
    };

    static void onConnect(esp_websocket_event_data_t &arg, SigmaWsClient *ws);
    void sendAuthMessage();
    static void onDisconnect(esp_websocket_event_data_t &arg, SigmaWsClient *ws);
    void onDataText(String &payload, SigmaWsClient *ws);
    void onDataBinary(byte *payload, size_t payloadLen, SigmaWsClient *ws);
    void onData(esp_websocket_event_data_t &arg, SigmaWsClient *ws);
    void onError(esp_websocket_event_data_t &arg, SigmaWsClient *ws);
    // static void onTimeout(void *arg, AsyncClient *c, uint32_t time);
    bool sendWebSocketFrame(const byte *payload, size_t payloadLen, byte opcode, bool isAuth = false);
    void setReady(bool ready);
    static void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void wsEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    bool sendWebSocketTextFrame(const String &payload, bool isAuth = false);
    bool sendWebSocketBinaryFrame(const byte *data, size_t size);
    bool sendWebSocketPingFrame(const String &payload);
    bool sendWebSocketPongFrame(const String &payload);
    bool sendWebSocketCloseFrame();
};

#endif