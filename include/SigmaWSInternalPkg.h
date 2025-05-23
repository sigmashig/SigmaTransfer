#ifndef SIGMAWSINTERNALPKG_H
#define SIGMAWSINTERNALPKG_H
#include <Arduino.h>
#include "SigmaAsyncNetwork.h"

#pragma once

class SigmaWSInternalPkg
{
    // This class is used to send internal messages to the Sigmw WebSocketServer protocol.
    // format:
    // {clientId:"client id named by client",
    //   data:"string or base64 for binary",
    //   length:"length of the data",
    //   type:PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE,PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE,PROTOCOL_RECEIVED_SIGMA_MESSAGE
    // }

public:
    //SigmaWSInternalPkg(String clientId, String data, int type);
    static String GetEncoded(byte *data, int length);
    static int GetDecodedLength(String data);
    static int GetDecoded(String data, byte *encodedData);

private:
};

#endif