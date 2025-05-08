#ifndef SIGMAMQTTPKG_H
#define SIGMAMQTTPKG_H

#pragma once
#include <Arduino.h>

#define MQTTPKG_SEPARATOR 0x01
class SigmaMQTTPkg
{
public:
    SigmaMQTTPkg(String topic, String payload);
    SigmaMQTTPkg(const char *msg);
    ~SigmaMQTTPkg();
    String GetTopic(){return topic;};
    String GetPayload(){return payload;};
    char *GetMsg(){return msg;};

private:
    String topic;
    String payload;
    char *msg;
};

#endif