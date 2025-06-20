#pragma once
#include <Arduino.h>
#include <WiFi.h>

typedef enum
{
    PROTOCOL_CONNECTED = 0x1000,
    PROTOCOL_DISCONNECTED,
    PROTOCOL_AP_CONNECTED,
    PROTOCOL_STA_CONNECTED,
    PROTOCOL_AP_DISCONNECTED,
    PROTOCOL_STA_DISCONNECTED,
    PROTOCOL_ERROR,
    // PROTOCOL_AUTH_SUCCESS,
    // PROTOCOL_AUTH_FAILED,
    PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE,
    PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE,
    PROTOCOL_RECEIVED_SIGMA_MESSAGE,
    PROTOCOL_RECEIVED_PING,
    PROTOCOL_RECEIVED_PONG,
    PROTOCOL_SEND_RAW_TEXT_MESSAGE,
    PROTOCOL_SEND_RAW_BINARY_MESSAGE,
    PROTOCOL_SEND_SIGMA_MESSAGE,
    PROTOCOL_SEND_PING,
    PROTOCOL_SEND_PONG,
} EVENT_IDS;

/*
typedef enum
{
    SIGMAASYNCNETWORK_MODE_NONE = 0,
    SIGMAASYNCNETWORK_MODE_STA,
    SIGMAASYNCNETWORK_MODE_AP,
    SIGMAASYNCNETWORK_MODE_STA_AP,

} WorkMode;
*/

typedef struct
{
    String ssid;
    String password;
} WiFiConfigSta;

typedef struct
{
    String ssid;
    String password;
} WiFiConfigAp;

typedef struct
{
    WiFiConfigSta wifiSta;
    WiFiConfigAp wifiAp;
    String modeString;
    wifi_mode_t wifiMode = WIFI_MODE_STA;
    bool enabled = true;
} WiFiConfig;

typedef struct
{
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    bool enabled = true;
} EthernetConfig;

typedef struct
{
    WiFiConfig wifiConfig;
    EthernetConfig ethernetConfig;
    bool enabled = true;
} NetworkConfig;

typedef struct
{
    String clientId = "";
    String payload = "";
    bool isBinary = false;
    String topic = "";
    byte *binaryPayload = nullptr;
    int binaryPayloadLength = 0;
} SigmaInternalStruct;

typedef struct
{
    String server;
    uint16_t port = 1883;
    String rootTopic;
    String username = "";
    String password = "";
    String clientId = "Client_" + String(ESP.getEfuseMac(), HEX);
    int keepAlive = 60;
    bool enabled = true;
} MqttConfig;

typedef struct
{
    String topic;
    int32_t eventId = PROTOCOL_SEND_SIGMA_MESSAGE;
    bool isReSubscribe = true;
} TopicSubscription;

typedef struct
{
    byte *data;
    size_t size;
} BinaryData;

typedef enum
{
    AUTH_TYPE_NONE = 0,
    AUTH_TYPE_URL = 0x01,
    AUTH_TYPE_BASIC = 0x02,
    AUTH_TYPE_FIRST_MESSAGE = 0x04,
    AUTH_TYPE_ALL_MESSAGES = 0x08 // All messages are authenticated. No Binary Data. Text/Sigma Only. Converted to Json
} AuthType;

typedef struct
{
    String host;
    uint16_t port = 80;
    String clientId = "WSClient_" + String(ESP.getEfuseMac(), HEX);
    String rootPath = "/";
    String apiKey = "";
    byte authType = AUTH_TYPE_NONE;
    bool enabled = true;
} WSClientConfig;

typedef struct
{
    uint16_t port = 80;
    String rootPath = "/";
    // String apiKey = "";
    byte authType = AUTH_TYPE_NONE;
    byte maxClients = 10;
    byte maxConnectionsPerClient = 1;
    bool enabled = true;
} WSServerConfig;

typedef enum
{
    SIGMA_PROTOCOL_MQTT = 0,
    SIGMA_PROTOCOL_WS_SERVER,
    SIGMA_PROTOCOL_WS_CLIENT,
    SIGMA_PROTOCOL_UNKNOWN
} SigmaProtocolType;

typedef struct
{
    String typeString;
    SigmaProtocolType type;
    MqttConfig mqttConfig;
    WSClientConfig wsClientConfig;
    WSServerConfig wsServerConfig;
} SigmaConnectionsConfig;