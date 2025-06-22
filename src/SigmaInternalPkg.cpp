#include "SigmaInternalPkg.h"
#include <ArduinoJson.h>
#include <libb64/cdecode.h>
#include <libb64/cencode.h>

SigmaInternalPkg::SigmaInternalPkg(String topic, String payload, byte qos, bool retained, bool isBinary, String clientId)
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

    init(topic, payload, qos, retained, isBinary, clientId, binaryPayload, length);
}

SigmaInternalPkg::SigmaInternalPkg(String topic, byte *binaryPayload, int binaryPayloadLength, byte qos, bool retained, bool isBinary, String clientId)
{
    int length;
    String payload = "";

    length = GetEncodedLength(binaryPayloadLength);
    payload = GetEncoded(binaryPayload, binaryPayloadLength);
    this->isAllocated = false;

    init(topic, payload, qos, retained, isBinary, clientId, binaryPayload, binaryPayloadLength);
}

void SigmaInternalPkg::init(String topic, String payload, byte qos, bool retained, bool isBinary, String clientId, byte *binaryPayload, int binaryPayloadLength)
{
    pkgData.topic = topic;
    pkgData.payload = payload;
    pkgData.isBinary = isBinary;
    pkgData.clientId = clientId;
    pkgData.binaryPayloadLength = binaryPayloadLength;
    pkgData.binaryPayload = binaryPayload;
    pkgData.qos = qos;
    pkgData.retained = retained;
    JsonDocument doc;
    doc["clientId"] = pkgData.clientId;
    doc["topic"] = pkgData.topic;
    doc["payload"] = pkgData.payload;
    doc["isBinary"] = pkgData.isBinary;
    doc["binaryPayloadLength"] = pkgData.binaryPayloadLength;
    doc["qos"] = pkgData.qos;
    doc["retained"] = pkgData.retained;
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
        Serial.printf("Error deserializing JSON: %s\n", error.c_str());
        this->isError = true;
        return;
    }
    int length = 0;
    SigmaInternalStruct pkg0;
    pkg0.topic = doc["topic"].as<String>();
    pkg0.payload = doc["payload"].as<String>();
    if (doc["clientId"].is<String>()) {
    pkg0.clientId = doc["clientId"].as<String>();
    } else {
        pkg0.clientId = "";
    }
    if (doc["qos"].is<byte>()) {
        pkg0.qos = doc["qos"].as<byte>();
    } else {
        pkg0.qos = 0;
    }
    if (doc["retained"].is<bool>()) {
        pkg0.retained = doc["retained"].as<bool>();
    } else {
        pkg0.retained = false;
    }
    if (doc["binaryPayloadLength"].is<int>()) {
        pkg0.binaryPayloadLength = doc["binaryPayloadLength"].as<int>();
    } else {
        pkg0.binaryPayloadLength = 0;
    }
    if (doc["isBinary"].as<bool>())
    { // the payload contains an encoded binary
        pkg0.isBinary = doc["isBinary"].as<bool>();
        length = GetDecodedLength(doc["payload"].as<String>().length());
        pkg0.binaryPayload = (byte *)malloc(length);
        this->isAllocated = true;
        pkg0.binaryPayloadLength =  GetDecoded(doc["payload"].as<String>(), binaryPayload);
    }
    else
    {
        this->isAllocated = false;
        binaryPayload = nullptr;
    }
    // Serial.printf("payload: %s\n", doc["payload"].as<String>().c_str());
    init(pkg0.topic, pkg0.payload, pkg0.qos, pkg0.retained, pkg0.isBinary, pkg0.clientId, pkg0.binaryPayload, pkg0.binaryPayloadLength);
}

SigmaInternalPkg::~SigmaInternalPkg()
{
    if (this->binaryPayload != nullptr && this->isAllocated)
    {
        free(this->binaryPayload);
    }
}

bool SigmaInternalPkg::IsSigmaInternalPkg(const String &json)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error)
    {
        Serial.printf("[check]Error deserializing JSON: %s\n", error.c_str());
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