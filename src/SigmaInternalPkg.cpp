#include "SigmaInternalPkg.h"
#include <ArduinoJson.h>
#include <libb64/cdecode.h>
#include <libb64/cencode.h>

SigmaInternalPkg::SigmaInternalPkg(String protocol, String topic, String payload)
{
    this->protocol = protocol;
    this->topic = topic;
    this->payload = payload;
    this->isBinary = false;
    this->length = payload.length();
    this->binaryPayload = nullptr;
    this->binaryPayloadLength = 0;

    JsonDocument doc;
    doc["protocol"] = protocol;
    doc["topic"] = topic;
    doc["payload"] = payload;
    doc["isBinary"] = isBinary;
    doc["length"] = payload.length();
    serializeJson(doc, this->pkgString);
}

SigmaInternalPkg::SigmaInternalPkg(String protocol, String topic, byte *binaryPayload, int binaryPayloadLength)
{
    this->protocol = protocol;
    this->topic = topic;
    this->binaryPayload = binaryPayload;
    this->binaryPayloadLength = binaryPayloadLength;

    char *buffer;
    int bufferLen = base64_encode_expected_len(binaryPayloadLength);
    buffer = (char *)malloc(bufferLen);

    base64_encode_chars((char *)binaryPayload, binaryPayloadLength, buffer);
    this->payload = String(buffer);
    free(buffer);
    JsonDocument doc;
    doc["protocol"] = protocol;
    doc["topic"] = topic;
    doc["payload"] = payload;
    doc["isBinary"] = true;
    doc["length"] = binaryPayloadLength;
    // String jsonString = "";
    serializeJson(doc, this->pkgString);
}

SigmaInternalPkg::SigmaInternalPkg(const char *msg)
{
    JsonDocument doc;
    deserializeJson(doc, msg);
    if (doc["protocol"].is<String>()) {
        this->protocol = doc["protocol"].as<String>();
    }
    else
    {
        this->isError = true;
    }
    if (doc["topic"].is<String>())
    {
        this->topic = doc["topic"].as<String>();
    }
    else
    {
        this->isError = true;
    }
    if (doc["isBinary"].is<bool>())
    {
        this->isBinary = doc["isBinary"].as<bool>();
    }
    else
    {
        this->isError = true;
    }
    if (doc["length"].is<int>())
    {
        this->length = doc["length"].as<int>();
    }
    else
    {
        this->isError = true;
    }
    if (this->isBinary && !this->isError)
    {
        this->binaryPayload = (byte *)malloc(this->length);
        this->isAllocated = true;
        base64_decode_chars(this->payload.c_str(), this->length, (char *)this->binaryPayload);
    }
    else
    {
        this->payload = doc["payload"].as<String>();
    }
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
    if (error) {
        return false;
    }
    if (!doc["protocol"].is<String>()) {
        return false;
    }
    if (!doc["topic"].is<String>()) {
        return false;
    }
    if (!doc["payload"].is<String>()) {
        return false;
    }
    //   if (!doc["isBinary"].is<bool>()) {
    //     return false;
    // }
    // if (!doc["length"].is<int>()) {
    //     return false;
    // }
    return true;
}
