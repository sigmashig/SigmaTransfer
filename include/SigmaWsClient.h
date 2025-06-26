#ifndef SIGMAWSCLIENT_H
#define SIGMAWSCLIENT_H

#pragma once
#include <Arduino.h>
#include <SigmaLoger.h>
#include <SigmaInternalPkg.h>
#include <map>
#include <esp_event.h>
#include "SigmaConnection.h"
#include <AsyncTCP.h>
#include "SigmaTransferDefs.h"
#include <AsyncWebSocket.h>

class SigmaWsClient : public SigmaConnection
{
public:
    SigmaWsClient(WSClientConfig config, SigmaLoger *logger = nullptr, uint priority = 5);
    // void Subscribe(TopicSubscription subscriptionTopic);
    // void Unsubscribe(String topic);

private:
    void Connect();
    void Disconnect();
    void Close()
    {
        shouldConnect = false;
        Disconnect();
    };

    WSClientConfig config;

    bool shouldConnect = true;
    inline static AsyncClient wsClient;
    static void onConnect(void *arg, AsyncClient *c);
    void sendAuthMessage();
    static void onDisconnect(void *arg, AsyncClient *c);
    static void onData(void *arg, AsyncClient *c, void *data, size_t len);
    static void onError(void *arg, AsyncClient *c, int8_t error);
    static void onTimeout(void *arg, AsyncClient *c, uint32_t time);
    bool sendWebSocketFrame(const byte *payload, size_t payloadLen, byte opcode, bool isAuth = false);
    void setReady(bool ready);
    static void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    bool sendWebSocketTextFrame(const String &payload, bool isAuth = false);
    bool sendWebSocketBinaryFrame(const byte *data, size_t size);
    bool sendWebSocketPingFrame(const String &payload);
    bool sendWebSocketPongFrame(const String &payload);
    bool sendWebSocketCloseFrame();
};

#endif