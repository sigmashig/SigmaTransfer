#ifndef SIGMAUART_H
#define SIGMAUART_H

#pragma once
#include <Arduino.h>
#include <SigmaLoger.h>
#include <SigmaInternalPkg.h>
#include <map>
#include <esp_event.h>
#include "SigmaProtocolDefs.h"
#include "SigmaProtocol.h"
#include <HardwareSerial.h>
// #include <driver/uart.h>

typedef struct
{
    byte uartNum;
    uint txPin;
    uint rxPin;
    ulong baudRate;
    SerialConfig portConfig = SERIAL_8N1;
} UartConfig;

ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

class SigmaUART : public SigmaProtocol
{
public:
    SigmaUART(UartConfig config);
    SigmaUART();

    void SetParams(UartConfig config);
    bool BeginSetup();
    bool FinalizeSetup();
    void Subscribe(TopicSubscription subscriptionTopic);
    void Subscribe(String topic)
    {
        TopicSubscription pkg;
        pkg.topic = topic;
        Subscribe(pkg);
    };
    void send(String topic, String payload);
    void Unsubscribe(String topic);
    void Unsubscribe(TopicSubscription topic) { Unsubscribe(topic.topic); };
    void SetShouldConnect(bool shouldConnect) { this->shouldConnect = shouldConnect; };
    bool GetShouldConnect() { return shouldConnect; };
    void Close()
    {
        shouldConnect = false;
        Disconnect();
    };

    // void SetClientId(String id) { clientId= id; };
    void Connect();
    void Disconnect();
    bool IsReady() { return isConnected; };
    bool IsNetRequired() { return false; };
    String GetName() { return name; };
    void SetName(String name)
    {
        Serial.printf("SetName=%s\n", name.c_str());
        this->name = name;
    };

private:
    UartConfig config;
    // inline static SigmaLoger *MLogger = new SigmaLoger(512);
    bool isConnected = false;
    static void onReceiveMsg();
    static void onReceiveError(hardwareSerial_error_t error);

    inline static String name;
    inline static String clientId;
    inline static std::map<String, TopicSubscription> eventMap;

    inline static HardwareSerial *serial;
    inline static bool shouldConnect = true;
};

#endif