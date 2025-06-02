#include "SigmaInternalPkg.h"
#include <ArduinoJson.h>
#include <libb64/cdecode.h>
#include <libb64/cencode.h>

SigmaInternalPkg::SigmaInternalPkg(String topic, String payload, bool isBinary, String clientId)
{
    int length;
    byte *binaryPayload = nullptr;

    if (isBinary)
    { // the payload contains an encoded binary
        length = GetDecodedLength(payload.length());
        binaryPayload = (byte *)malloc(length);
        this->isAllocated = true;
        length = GetDecoded(payload, pkgData.binaryPayload);
    }
    else
    {
        this->isAllocated = false;
        length = payload.length();
        binaryPayload = nullptr;
    }

    init(topic, payload, isBinary, clientId, binaryPayload, length);
}

SigmaInternalPkg::SigmaInternalPkg(String topic, byte *binaryPayload, int binaryPayloadLength, bool isBinary, String clientId)
{
    int length;
    String payload = "";

    length = GetEncodedLength(binaryPayloadLength);
    payload = GetEncoded(binaryPayload, binaryPayloadLength);
    this->isAllocated = false;

    init(topic, payload, isBinary, clientId, binaryPayload, binaryPayloadLength);
}

void SigmaInternalPkg::init(String topic, String payload, bool isBinary, String clientId, byte *binaryPayload, int binaryPayloadLength)
{
    // pkgData.protocol = protocol;
    pkgData.topic = topic;
    pkgData.isBinary = isBinary;
    pkgData.clientId = clientId;
    pkgData.binaryPayloadLength = binaryPayloadLength;
    pkgData.binaryPayload = binaryPayload;
    JsonDocument doc;
    doc["clientId"] = pkgData.clientId;
    // doc["protocol"] = pkgData.protocol;
    doc["topic"] = pkgData.topic;
    doc["payload"] = pkgData.payload;
    doc["isBinary"] = pkgData.isBinary;
    doc["binaryPayloadLength"] = pkgData.binaryPayloadLength;
    serializeJson(doc, pkgString);
}

SigmaInternalPkg::SigmaInternalPkg(const char *msg)
{
    if (!IsSigmaInternalPkg(msg))
    {
        this->isError = true;
        return;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (error)
    {
        this->isError = true;
        return;
    }
    int length = 0;
    byte *binaryPayload = nullptr;
    if (doc["isBinary"].as<bool>())
    { // the payload contains an encoded binary
        length = GetDecodedLength(doc["payload"].as<String>().length());
        binaryPayload = (byte *)malloc(length);
        this->isAllocated = true;
        length = GetDecoded(doc["payload"].as<String>(), binaryPayload);
    }
    else
    {
        this->isAllocated = false;
        binaryPayload = nullptr;
    }

    init(doc["topic"].as<String>(), doc["payload"].as<String>(), doc["isBinary"].as<bool>(), doc["clientId"].as<String>(), binaryPayload, length);
}

SigmaInternalPkg::~SigmaInternalPkg()
{
    if (this->binaryPayload != nullptr && this->isAllocated)
    {
        free(this->binaryPayload);
    }
}

bool SigmaInternalPkg::IsSigmaInternalPkg(const String &msg)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (error)
    {
        Serial.printf("Error deserializing JSON: %s\n", error.c_str());
        return false;
    }
    if (!doc["topic"].is<String>())
    {
        //    Serial.println("topic not found");
        return false;
    }
    if (!doc["payload"].is<String>())
    {
        //    Serial.println("payload not found");
        return false;
    }

    return true;
}

int SigmaInternalPkg::GetEncodedLength(int length)
{
    return base64_encode_expected_len(length);
}

String SigmaInternalPkg::GetEncoded(byte *data, int length)
{
    String msg = "";
    char *buffer;
    int bufferLen = base64_encode_expected_len(length);
    buffer = (char *)malloc(bufferLen);
    base64_encode_chars((char *)data, length, buffer);
    msg = String(buffer);
    free(buffer);

    return msg;
}

int SigmaInternalPkg::GetDecodedLength(int length)
{

    return base64_decode_expected_len(length);
}

int SigmaInternalPkg::GetDecoded(String data, byte *encodedData)
{
    int length = GetDecodedLength(data.length());
    return base64_decode_chars(data.c_str(), length, (char *)encodedData);
}