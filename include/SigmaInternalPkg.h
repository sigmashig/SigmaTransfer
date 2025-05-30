#ifndef SIGMAINTERNALPKG_H
#define SIGMAINTERNALPKG_H

#pragma once
#include <Arduino.h>

// #define INTERNALPKG_TOPIC_SEPARATOR 0x01
// #define INTERNALPKG_MESSAGE_END 0x00
typedef struct
{
    String clientId="";
    String payload="";
    bool isBinary=false;
    String protocol="";
    String topic="";
    byte *binaryPayload=nullptr;
    int binaryPayloadLength=0;
} SigmaInternalStruct;

class SigmaInternalPkg
{
    // This class is used to send internal messages to the Sigma protocol.
    // format:
    // {protocol:"name of the protocol",
    //      "topic":"the topic",
    //      "isBinary":"true or false",
    //      "payload":"the payload",
    //      "length":"length of the payload",
    //      "clientId":"the client id"
    //      }
public:
    // Raw message
    // String message plain or encoded binary
    SigmaInternalPkg(String protocol, String topic, String payload, bool isBinary = false, String clientId = "");
    // binary message. The message will be sent as binary when isBinary is true and will sent as text and the encoded binary when isBinary is false 
    SigmaInternalPkg(String protocol, String topic, byte *binaryPayload, int binaryPayloadLength, bool isBinary = false, String clientId = "");


    // prepare for deserialize
    // msg is the message received from the Sigma protocol JSON format
    SigmaInternalPkg(const char *msg);
    SigmaInternalPkg(const String &msg) : SigmaInternalPkg(msg.c_str()) {};
    // copy constructor
    //SigmaInternalPkg(SigmaInternalStruct pkg) : SigmaInternalPkg(pkg.protocol, pkg.topic, pkg.payload, pkg.isBinary, pkg.clientId) {};

    ~SigmaInternalPkg();

    String GetTopic() { return pkgData.topic; };
    String GetPayload() { return pkgData.payload; };
    String GetProtocol() { return pkgData.protocol; };
    String GetPkgString() { return pkgString; };
    int GetBinaryPayloadLength() { return pkgData.binaryPayloadLength; };
    byte *GetBinaryPayload() { return (byte *)pkgData.payload.c_str(); };
    String GetClientId() { return pkgData.clientId; };
    bool IsBinary() { return pkgData.isBinary; };
    bool IsError() { return isError; };
    static bool IsSigmaInternalPkg(const String &msg);
    static int GetEncodedLength(int binaryLength);
    static String GetEncoded(byte *data, int length);
    static int GetDecodedLength(int length);
    static int GetDecoded(String data, byte *encodedData);

private:
    SigmaInternalStruct pkgData;

    //   int length;
    byte *binaryPayload = nullptr;
    int binaryPayloadLength;
    String pkgString;

    bool isAllocated = false;
    bool isError = false;

    void init(String protocol, String topic, String payload, bool isBinary, String clientId, byte *binaryPayload, int binaryPayloadLength);
};

#endif