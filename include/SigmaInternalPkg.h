#ifndef SIGMAINTERNALPKG_H
#define SIGMAINTERNALPKG_H

#pragma once
#include <Arduino.h>

#define INTERNALPKG_TOPIC_SEPARATOR 0x01
#define INTERNALPKG_MESSAGE_END 0x00

class SigmaInternalPkg
{
public:
    SigmaInternalPkg(String topic, String payload);
    SigmaInternalPkg(const char *msg);
    SigmaInternalPkg(String msg) : SigmaInternalPkg(msg.c_str()) {};
    ~SigmaInternalPkg();
    String GetTopic(){return topic;};
    String GetPayload(){return payload;};
    char *GetMsg(){return msg;};
    int GetMsgLength(){return msgLength;};
private:
    String topic;
    String payload;
    char *msg;
    int msgLength;
};

#endif