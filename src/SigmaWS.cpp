#include "SigmaWS.h"
#include <ArduinoJson.h>
#include <libb64/cencode.h>

ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

SigmaWS::SigmaWS(WSConfig _config) : SigmaProtocol(config.)
{
    config = _config;
    wsClient.onConnect(onConnect, this);
    wsClient.onDisconnect(onDisconnect, this);
    wsClient.onData(onData, this);
    wsClient.onError(onError, this);
    wsClient.onTimeout(onTimeout, this);
}

SigmaWS::SigmaWS()
{
}

void SigmaWS::SetParams(WSConfig _config)
{
    config = _config;
}

bool SigmaWS::BeginSetup()
{
    return false;
}

bool SigmaWS::FinalizeSetup()
{
    return false;
}

void SigmaWS::Connect()
{
    MLogger->Internal("WS connecting");
    String url = config.host;
    if (config.authType & AUTH_TYPE_URL)
    {
        url += "?apiKey=" + config.apiKey;
    }
    wsClient.connect(url.c_str(), config.port);
}

void SigmaWS::onConnect(void *arg, AsyncClient *c)
{
    SigmaWS *ws = (SigmaWS *)arg;
    MLogger->Internal("TCP connected");
    // wsConnected = true;
    // wsHandshakeComplete = false;
    // wsBuffer = "";

    // Generate WebSocket key - random 16 bytes base64 encoded
    byte randomKey[16];
    for (int i = 0; i < 16; i++)
    {
        randomKey[i] = random(0, 256);
    }

    // Base64 encode the key
    // String wsKey =  base64Encode(randomKey, 16);
    char *buffer;
    int bufferLen = base64_encode_expected_len(16);
    buffer = (char *)malloc(bufferLen);
    base64_encode_chars((char *)randomKey, 16, buffer);
    String wsKey = String(buffer);
    free(buffer);
    MLogger->Append("Generated WebSocket Key: ").Append(wsKey).Internal();

    // Create HTTP upgrade request
    String handshake = "GET ";
    handshake += ws->config.rootTopic;
    handshake += " HTTP/1.1\r\n";
    handshake += "Host: ";
    handshake += ws->config.host;
    handshake += ":";
    handshake += ws->config.port;
    handshake += "\r\n";
    handshake += "Connection: Upgrade\r\n";
    handshake += "Upgrade: websocket\r\n";
    handshake += "Sec-WebSocket-Version: 13\r\n";
    handshake += "Sec-WebSocket-Key: ";
    handshake += wsKey;
    handshake += "\r\n";

    if (ws->config.authType & AUTH_TYPE_BASIC)
    {

        handshake += "Authorization: Basic ";
        char *buffer;
        int bufferLen = base64_encode_expected_len(ws->config.apiKey.length());
        buffer = (char *)malloc(bufferLen);
        base64_encode_chars((char *)(ws->config.apiKey.c_str()), ws->config.apiKey.length(), buffer);
        handshake += String(buffer);
        free(buffer);
        handshake += "\r\n";
    }
    handshake += "\r\n";

    MLogger->Append("Sending handshake:").Append(handshake).Internal();

    // Send handshake
    wsClient.add(handshake.c_str(), handshake.length());
    wsClient.send();
    // IsReady set after first handshake

    // ws->isReady = true;
    if (ws->config.authType & (AUTH_TYPE_BASIC | AUTH_TYPE_URL | AUTH_TYPE_NONE))
    {
        ws->setReady(true);
    }
    MLogger->Append("Sent WebSocket handshake").Internal();
}

void SigmaWS::setReady(bool ready)
{
    isReady = ready;
    esp_event_post_to(eventLoop, SIGMATRANSFER_EVENT, isReady ? PROTOCOL_CONNECTED : PROTOCOL_DISCONNECTED, (void *)(GetName().c_str()), GetName().length() + 1, portMAX_DELAY);
}

void SigmaWS::onDisconnect(void *arg, AsyncClient *c)
{
    MLogger->Append("TCP disconnected").Internal();
    SigmaWS *ws = (SigmaWS *)arg;
    ws->setReady(false);
}

void SigmaWS::onData(void *arg, AsyncClient *c, void *data, size_t len)
{
    SigmaWS *ws = (SigmaWS *)arg;
    MLogger->Append("Received data").Internal();
    char *buf = (char *)data;
    String response = String(buf, len);
    MLogger->Append("Received data from WebSocket server").Internal();
    MLogger->Append(response).Internal();
    MLogger->Append("**************************************************").Internal();

    if (response.indexOf("HTTP/1.1 101") >= 0 &&
        response.indexOf("Upgrade: websocket") >= 0)
    {
        MLogger->Append("WebSocket handshake successful").Internal();
        if (!ws->isReady && ws->config.authType & (AUTH_TYPE_FIRST_MESSAGE))
        {
            JsonDocument doc;
            doc["type"] = "auth";
            doc["apiKey"] = ws->config.apiKey;
            doc["client"] = ws->config.clientId;
            String jsonStr;
            serializeJson(doc, jsonStr);
            ws->Send(jsonStr);
            ws->setReady(true);
        }
        else
        {
            // Parse WebSocket frame
            MLogger->Append("Received data from WebSocket server").Internal();
            byte *bytes = (byte *)data;
            byte opcode = bytes[0] & 0x0F;
            bool masked = (bytes[1] & 0x80) != 0;
            ulong payload_len = bytes[1] & 0x7F;

            // Basic WebSocket frame parsing - this is simplified
            if (len > 2)
            {
                bool fin = (bytes[0] & 0x80) != 0;

                int headerLen = 2;
                if (payload_len == 126)
                {
                    if (len < 4)
                        return; // Not enough data
                    payload_len = ((uint16_t)bytes[2] << 8) | bytes[3];
                    headerLen += 2;
                }
                else if (payload_len == 127)
                {
                    if (len < 10)
                        return; // Not enough data
                    payload_len = 0;
                    for (int i = 0; i < 8; i++)
                    {
                        payload_len = (payload_len << 8) | bytes[2 + i];
                    }
                    headerLen += 8;
                }

                // Skip masking key if present
                if (masked)
                {
                    headerLen += 4;
                }

                // Extract payload if we have enough data
                if (len >= headerLen + payload_len)
                {
                    switch (opcode)
                    {
                    case 0x01:
                    {
                        // Text frame
                        String payload;

                        // Unmask if needed
                        if (masked)
                        {
                            uint8_t mask[4];
                            memcpy(mask, bytes + headerLen - 4, 4);

                            payload.reserve(payload_len);
                            for (size_t i = 0; i < payload_len; i++)
                            {
                                payload += (char)(bytes[headerLen + i] ^ mask[i % 4]);
                            }
                        }
                        else
                        {
                            payload = String((char *)(bytes + headerLen), payload_len);
                        }

                        MLogger->Append("Received: ").Append(payload).Internal();
                        PostEvent(PROTOCOL_MESSAGE, (void *)(payload.c_str()), payload.length());

                        if (ws->config.textFrameType == TEXT_FRAME_TYPE_PARSE)
                        { // Parse JSON. find "topic" and "payload"
                            JsonDocument doc;
                            DeserializationError error = deserializeJson(doc, payload);

                            if (!error)
                            {
                                String topic = "";
                                String payload = "";
                                if (doc["topic"].is<String>())
                                {
                                    topic = doc["topic"].as<String>();
                                }
                                if (doc["payload"].is<String>())
                                {
                                    payload = doc["payload"].as<String>();
                                }
                                PostEvent(PROTOCOL_MESSAGE, (void *)(payload.c_str()), payload.length());
                            }
                        }
                        break;
                    }
                    case 0x02:
                    { // Binary frame
                        byte *payload;

                        // Unmask if needed
                        if (masked)
                        {
                            uint8_t mask[4];
                            memcpy(mask, bytes + headerLen - 4, 4);

                            payload = (byte *)malloc(payload_len);
                            for (size_t i = 0; i < payload_len; i++)
                            {
                                payload[i] = (bytes[headerLen + i] ^ mask[i % 4]);
                            }
                        }
                        else
                        {
                            payload = (byte *)malloc(payload_len);
                            memcpy(payload, bytes + headerLen, payload_len);
                        }

                        PostEvent(PROTOCOL_MESSAGE, payload, payload_len);
                        free(payload);
                        break;
                    }
                    case 0x08:
                    {
                        // Close frame. Server is closing the connection. We will try to reconnect
                        MLogger->Append("Received close frame").Internal();
                        ws->setReady(false);
                        PostEvent(PROTOCOL_DISCONNECTED, NULL, 0);
                        ws->Connect();
                        break;
                    }
                    case 0x09:
                    {
                        // Ping frame. Send pong
                        MLogger->Append("Received ping frame").Internal();
                        ws->SendWebSocketFrame("", 0x0A); // Pong
                        break;
                    }
                    case 0x0A:
                    {
                        // Pong frame
                        MLogger->Append("Received pong frame").Internal();
                        PostEvent(PROTOCOL_PONG, NULL, 0);
                        break;
                    }
                    default:
                    {
                        // Unknown frame
                        MLogger->Append("Unknown frame").Internal();
                        break;
                    }
                    }
                }
            }
            else
            {
                MLogger->Append("WebSocket handshake failed").Internal();
                String error = "[" + ws->GetName() + "] " + "WebSocket handshake failed";
                ws->PostEvent(PROTOCOL_ERROR, (void *)(error.c_str()), error.length());
            }
        }
    }
}

void SigmaWS::onError(void *arg, AsyncClient *c, int8_t error)
{
    SigmaWS *ws = (SigmaWS *)arg;
    String errorStr = "[" + ws->GetName() + "] " + "TCP error: " + error;
    MLogger->Append(errorStr).Internal();
    ws->PostEvent(PROTOCOL_ERROR, (void *)errorStr.c_str(), errorStr.length());
}

void SigmaWS::onTimeout(void *arg, AsyncClient *c, uint32_t time)
{
    SigmaWS *ws = (SigmaWS *)arg;
    String errorStr = "[" + ws->GetName() + "] " + "TCP timeout after " + String(time) + "ms";
    MLogger->Append(errorStr).Internal();
    ws->PostEvent(PROTOCOL_ERROR, (void *)errorStr.c_str(), errorStr.length());
    //    c->close();
}

/*
// Base64 encoding for WebSocket key
String SigmaWS::base64Encode(const byte *data, uint length)
{
    const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String encoded;
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    while (length--)
    {
        char_array_3[i++] = *(data++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                encoded += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++)
            encoded += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            encoded += '=';
    }

    return encoded;
}
*/

void SigmaWS::SendWebSocketFrame(const String &payload, uint8_t opcode)
{
    if (!isReady)
    {
        MLogger->Append("Cannot send message - not connected").Internal();
        return;
    }

    // Create WebSocket frame
    // Format: FIN=1, RSV1-3=0, Opcode=1(text) or 2(binary)
    // MASK=1 (client to server must be masked)
    // Payload length + mask + payload

    size_t headerSize = 2; // Basic header size
    size_t payloadLen = payload.length();

    // Extended payload length
    if (payloadLen > 125 && payloadLen < 65536)
    {
        headerSize += 2; // 16-bit length
    }
    else if (payloadLen >= 65536)
    {
        headerSize += 8; // 64-bit length
    }

    // Add mask size
    headerSize += 4; // Masking key

    // Create buffer
    uint8_t *buffer = new uint8_t[headerSize + payloadLen];

    // Set opcode and FIN bit
    buffer[0] = 0x80 | (opcode & 0x0F); // FIN=1, opcode

    // Set payload length and mask bit
    if (payloadLen <= 125)
    {
        buffer[1] = 0x80 | payloadLen; // MASK=1, length
    }
    else if (payloadLen < 65536)
    {
        buffer[1] = 0x80 | 126; // MASK=1, length=126
        buffer[2] = (payloadLen >> 8) & 0xFF;
        buffer[3] = payloadLen & 0xFF;
    }
    else
    {
        buffer[1] = 0x80 | 127; // MASK=1, length=127
        for (int i = 0; i < 8; i++)
        {
            buffer[2 + i] = (payloadLen >> (56 - 8 * i)) & 0xFF;
        }
    }

    // Generate random mask key
    uint8_t maskKey[4];
    for (int i = 0; i < 4; i++)
    {
        maskKey[i] = random(0, 256);
    }

    // Copy mask key
    memcpy(buffer + (headerSize - 4), maskKey, 4);

    // Copy and mask payload
    // Serial.print("Payload: ");
    // Serial.println(payload);
    // Serial.print("Buffer: ");
    for (size_t i = 0; i < payloadLen; i++)
    {
        buffer[headerSize + i] = payload[i] ^ maskKey[i % 4];
        // Serial.printf("%c(%02x)", buffer[headerSize + i], buffer[headerSize + i]);
    }
    // Serial.println();
    //  Debug output to verify what we're sending
    Serial.printf("Sending WebSocket frame: opcode=%d, length=%d\n", opcode, payloadLen);

    // Send frame
    wsClient.add((const char *)buffer, headerSize + payloadLen);
    wsClient.send();
    delete[] buffer;
}
