#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <map>
#include <new>
extern "C"
{
#include "driver/spi_master.h"
}

typedef enum
{
    PROTOCOL_CONNECTED = 0x1000,
    PROTOCOL_DISCONNECTED,
    PROTOCOL_ERROR,
    PROTOCOL_PING_TIMEOUT,
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
    PROTOCOL_SEND_RECONNECT,
    PROTOCOL_SEND_CLOSE,
} PROTOCOL_EVENTIDS;

typedef enum
{
    NETWORK_AP_CONNECTED = 0x100,  // this event is used for internal purposes. Please use NETWORK_LAN_CONNECTED instead
    NETWORK_STA_CONNECTED,         // this event is used for internal purposes. Please use NETWORK_WAN_CONNECTED instead
    NETWORK_AP_DISCONNECTED,       // this event is used for internal purposes. Please use NETWORK_LAN_DISCONNECTED instead
    NETWORK_STA_DISCONNECTED,      // this event is used for internal purposes. Please use NETWORK_WAN_DISCONNECTED instead
    NETWORK_ETHERNET_CONNECTED,    // this event is used for internal purposes. Please use NETWORK_WAN_CONNECTED instead
    NETWORK_ETHERNET_DISCONNECTED, // this event is used for internal purposes. Please use NETWORK_WAN_DISCONNECTED instead
    NETWORK_WAN_CONNECTED,
    NETWORK_WAN_DISCONNECTED,
    NETWORK_LAN_CONNECTED,
    NETWORK_LAN_DISCONNECTED

} NETWORK_EVENT_IDS;

typedef enum
{
    NETWORK_MODE_NONE = 0,
    NETWORK_MODE_WAN,
    NETWORK_MODE_LAN,
} NetworkMode;

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
    bool shouldReconnect = true;
} WiFiConfig;

typedef struct
{
    spi_host_device_t spiHost;
    int misoPin;
    int mosiPin;
    int sckPin;
    int csPin;
    uint spiClockMHz;
    int queueSize;
    spi_device_interface_config_t devcfg;
} SPIConfig;

enum EthernetHardwareType
{
    ETHERNET_HARDWARE_TYPE_W5500 = 0,
    ETHERNET_HARDWARE_TYPE_W5100,
    ETHERNET_HARDWARE_TYPE_W5200,
    ETHERNET_HARDWARE_TYPE_NONE = 0xFF
};

typedef struct W5500Config
{
    int rstPin;
    int intPin;
    SPIConfig spiConfig = {
        .spiHost = SPI3_HOST,
        .misoPin = GPIO_NUM_19,
        .mosiPin = GPIO_NUM_23,
        .sckPin = GPIO_NUM_18,
        .csPin = GPIO_NUM_5, // should be redefined
        .spiClockMHz = 20,
        .queueSize = 0, // TODO: automatically set
        .devcfg = {
            .command_bits = 16,
            .address_bits = 8,
            .mode = 0,
            .clock_speed_hz = 0, // automatically set by spiConfig.spiClockMHz when =0
            .spics_io_num = -1,  // automatically set by spiConfig.csPin when -1
        }};
    void ResetBoard()
    {
        pinMode(rstPin, OUTPUT);
        digitalWrite(rstPin, LOW);
        delay(10);
        digitalWrite(rstPin, HIGH);
        delay(50);
    }

} W5500Config;

struct EthernetConfig
{
    bool dhcp = true;
    IPAddress ip;
    IPAddress gateway;
    IPAddress dns;
    IPAddress subnet;
    bool enabled = true;
    byte mac[6] = {0};
    EthernetHardwareType hardwareType = ETHERNET_HARDWARE_TYPE_W5500;
    union EthernetHardware
    {
        W5500Config w5500;
        EthernetHardware()
        {
            new (&w5500) W5500Config();
        }
        ~EthernetHardware() {}
    } hardware;

    EthernetConfig()
        : dhcp(true), ip(), gateway(), dns(), subnet(), enabled(true), mac{0}, hardwareType(ETHERNET_HARDWARE_TYPE_W5500)
    {
    }
};

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
    byte qos = 0;
    bool retained = false;
} SigmaInternalStruct;

typedef struct
{
    String server;
    uint16_t port = 1883;
    NetworkMode networkMode = NETWORK_MODE_WAN;
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
typedef enum
{
    PING_NO = 0,
    PING_TEXT = 1,
    PING_BINARY = 2,
} PingType;

typedef struct
{
    String host;
    uint16_t port = 80;
    NetworkMode networkMode = NETWORK_MODE_WAN;
    String clientId = "WSClient_" + String(ESP.getEfuseMac(), HEX);
    String rootPath = "/";
    String apiKey = "";
    byte authType = AUTH_TYPE_NONE;
    bool enabled = true;
    int retryConnectingCount = 3;
    int retryConnectingDelay = 5; // 5 second
    PingType pingType = PING_NO;
    int pingInterval = 60;  // 60 seconds
    int pingRetryCount = 3; // reconnect after 3 pings
} WSClientConfig;

typedef struct
{
    String clientId;
    String authKey;
    //   PingType pingType = PING_TEXT;
} AllowableClient;

typedef struct
{
    uint16_t port = 80;
    NetworkMode networkMode = NETWORK_MODE_WAN;
    String rootPath = "/";
    byte authType = AUTH_TYPE_NONE;
    byte maxClients = 10;
    byte maxConnectionsPerClient = 1;
    bool enabled = true;
    std::map<String, AllowableClient> allowableClients;
    int pingInterval = 60;  // 60 seconds
    int pingRetryCount = 3; // disconnect after 3 pings
    PingType pingType = PING_TEXT;
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
    uint priority;

    MqttConfig mqttConfig;
    WSClientConfig wsClientConfig;
    WSServerConfig wsServerConfig;
} SigmaConnectionsConfig;
