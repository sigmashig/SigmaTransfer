#ifndef SIGMAWSINTERNALPKG_H
#define SIGMAWSINTERNALPKG_H
#include <Arduino.h>
#include "SigmaAsyncNetwork.h"
#include <ArduinoJson.h>

#pragma once
typedef struct
{
    String clientId;
    String data;
    int length;
    bool isBinary;
} SigmaWSServerStruct;

class SigmaWSServerPkg
{
    // This class is used to send internal messages to the Sigma WebSocketServer protocol.
    // format:
    // {clientId:"client id named by client",
    //   data:"string or base64 for binary",
    //   length:"length of the data",
    //   type:PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE,PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE,PROTOCOL_RECEIVED_SIGMA_MESSAGE
    // }

public:
    SigmaWSServerPkg(String clientId, String data);
    SigmaWSServerPkg(String clientId, byte *data, int length);
    SigmaWSServerPkg(JsonObject json);
    SigmaWSServerStruct GetStruct();
    static String GetEncoded(byte *data, int length);
    static int GetDecodedLength(String data);
    static int GetDecoded(String data, byte *encodedData);

private:
    SigmaWSServerStruct pkg;
};

#endif