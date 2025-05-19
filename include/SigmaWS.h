#ifndef SIGMAWS_H
#define SIGMAWS_H

#pragma once
#include <Arduino.h>
#include <SigmaLoger.h>
#include <SigmaInternalPkg.h>
#include <map>
#include <esp_event.h>
#include "SigmaProtocolDefs.h"
#include "SigmaProtocol.h"
#include <AsyncTCP.h>

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
    String rootTopic = "/";
    String apiKey = "";
    bool convertToJson = false;
    byte authType = AUTH_TYPE_NONE;
    byte textFrameType = TEXT_FRAME_TYPE_NOPARSE;
} WSConfig;

ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

class SigmaWS : public SigmaProtocol
{
public:
    SigmaWS(WSConfig config);
    SigmaWS();

    void SetParams(WSConfig config);
    bool BeginSetup();
    bool FinalizeSetup();
    void Subscribe(TopicSubscription subscriptionTopic);
    void Publish(String topic, String payload);
    void Unsubscribe(String topic);

    void SetClientId(String id) { clientId = id; };
    void Connect();
    void Disconnect();
    bool IsReady() { return isReady; };
    bool IsNetworkRequired() { return true; };
    String GetName() { return name; };
    void SetName(String name) { this->name = name; };
    void SetShouldConnect(bool shouldConnect) { this->shouldConnect = shouldConnect; };
    bool GetShouldConnect() { return shouldConnect; };
    void Close()
    {
        shouldConnect = false;
        Disconnect();
    };
    void SendWebSocketFrame(const String &payload, uint8_t opcode);

private:
    WSConfig config;
    inline static SigmaLoger *MLogger = new SigmaLoger(512);
    bool isReady = false;
    // bool wsConnected = false;
    // bool wsHandshakeComplete = false;

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
    //   static String base64Encode(const byte *data, uint length);
    void setReady(bool ready);
};

#endif