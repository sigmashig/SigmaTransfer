#ifndef SIGMAWS_H
#define SIGMAWS_H

#pragma once
#include <Arduino.h>
#include <SigmaLoger.h>
#include <SigmaInternalPkg.h>
#include <map>
#include <esp_event.h>
#include "SigmaProtocol.h"
#include <AsyncTCP.h>

// ESP_EVENT_DEFINE_BASE(PROTOCOL_WS);

enum AuthType
{
    AUTH_TYPE_NONE = 0,
    AUTH_TYPE_URL = 0x01,
    AUTH_TYPE_BASIC = 0x02,
    AUTH_TYPE_FIRST_MESSAGE = 0x04,
    AUTH_TYPE_ALL_MESSAGES = 0x08 // All messages are authenticated. convertToJson must be true
};
enum TextFrameType
{
    TEXT_FRAME_TYPE_NOPARSE = 0x01,
    TEXT_FRAME_TYPE_PARSE = 0x02,

};

typedef struct
{
    String host;
    uint16_t port = 80;
    String clientId;
    // String rootTopic = "/";
    String apiKey = "";
    bool convertToJson = false;
    byte authType = AUTH_TYPE_NONE;
    byte textFrameType = TEXT_FRAME_TYPE_NOPARSE;
} WSConfig;

class SigmaWS : public SigmaProtocol
{
public:
    SigmaWS(String name, WSConfig config);
    SigmaWS();
    void Subscribe(TopicSubscription subscriptionTopic);
    void Unsubscribe(String topic);

    void SetClientId(String id) { clientId = id; };
    void Connect();
    void Disconnect();
    bool IsNetworkRequired() { return true; };
    String GetName() { return name; };
    void Close()
    {
        shouldConnect = false;
        Disconnect();
    };

private:
    WSConfig config;

    inline static String name;
    inline static TimerHandle_t mqttReconnectTimer;
    inline static String clientId;

    inline static std::map<String, TopicSubscription> eventMap;
    inline static std::map<String, String> topicMsg;
    inline static bool shouldConnect = true;
    inline static AsyncClient wsClient;
    static void onConnect(void *arg, AsyncClient *c);
    static void onDisconnect(void *arg, AsyncClient *c);
    static void onData(void *arg, AsyncClient *c, void *data, size_t len);
    static void onError(void *arg, AsyncClient *c, int8_t error);
    static void onTimeout(void *arg, AsyncClient *c, uint32_t time);
    bool sendWebSocketFrame(const byte *payload, size_t payloadLen, byte opcode, bool isAuth = false);
    //   static String base64Encode(const byte *data, uint length);
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