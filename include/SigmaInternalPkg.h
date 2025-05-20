#ifndef SIGMAINTERNALPKG_H
#define SIGMAINTERNALPKG_H

#pragma once
#include <Arduino.h>

#define INTERNALPKG_TOPIC_SEPARATOR 0x01
#define INTERNALPKG_MESSAGE_END 0x00

class SigmaInternalPkg
{
    // This class is used to send internal messages to the Sigma protocol.
    // format:
    // {protocol:"name of the protocol", "topic":"the topic", "isBinary":"true or false", "payload":"the payload", "length":"length of the payload"}
public:
    // prepare for serialize
    SigmaInternalPkg(String protocol, String topic, String payload);
    SigmaInternalPkg(String protocol, String topic, byte *binaryPayload, int binaryPayloadLength);

    // prepare for deserialize
    // msg is the message received from the Sigma protocol JSON format
    SigmaInternalPkg(const char *msg);
    SigmaInternalPkg(const String &msg) : SigmaInternalPkg(msg.c_str()){};

    ~SigmaInternalPkg();

    String GetTopic() { return topic; };
    String GetPayload() { return payload; };
    String GetProtocol() { return protocol; };
    String GetPkgString() { return pkgString; };
    int GetBinaryPayloadLength() { return binaryPayloadLength; };
    byte *GetBinaryPayload() { return binaryPayload; };
    bool IsError() { return isError; };
    static bool IsSigmaInternalPkg(const String &msg);
private:
    String protocol;
    String topic;
    String payload;
    bool isBinary = false;
    int length;
    byte *binaryPayload = nullptr;
    int binaryPayloadLength;
    String pkgString;

    bool isAllocated = false;
    bool isError = false;
};

#endif