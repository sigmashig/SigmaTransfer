#include "SigmaWS.h"
#include <ArduinoJson.h>
#include <libb64/cencode.h>

// ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);
ESP_EVENT_DECLARE_BASE(SIGMAASYNCNETWORK_EVENT);

SigmaWS::SigmaWS(String name, WSConfig _config)
{
    config = _config;
    this->name = name;
    setRootTopic(config.rootTopic);
    wsClient.onConnect(onConnect, this);
    wsClient.onDisconnect(onDisconnect, this);
    wsClient.onData(onData, this);
    wsClient.onError(onError, this);
    wsClient.onTimeout(onTimeout, this);

    esp_event_handler_register_with(SigmaAsyncNetwork::GetEventLoop(), SIGMAASYNCNETWORK_EVENT, ESP_EVENT_ANY_ID, networkEventHandler, this);
    esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_RAW_BINARY_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_RAW_TEXT_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_SIGMA_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_PING, protocolEventHandler, this);
}

SigmaWS::SigmaWS()
{
}

void SigmaWS::protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaWS *ws = (SigmaWS *)arg;
    if (event_id == PROTOCOL_SEND_RAW_BINARY_MESSAGE)
    {
        BinaryData *data = (BinaryData *)event_data;

        ws->sendWebSocketBinaryFrame(data->data, data->size);
        // free(data); TODO: free
    }
    else if (event_id == PROTOCOL_SEND_RAW_TEXT_MESSAGE)
    {
        String *payload = (String *)event_data;
        ws->sendWebSocketTextFrame(*payload);
    }
    else if (event_id == PROTOCOL_SEND_SIGMA_MESSAGE)
    {
        SigmaInternalPkg *pkg = (SigmaInternalPkg *)event_data;
        ws->sendWebSocketTextFrame(pkg->GetPkgString());
    }
    else if (event_id == PROTOCOL_SEND_PING)
    {
        String *payload = (String *)event_data;
        ws->sendWebSocketPingFrame(*payload);
    }
}

void SigmaWS::networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaWS *ws = (SigmaWS *)arg;
    if (event_id == PROTOCOL_STA_CONNECTED)
    {
        ws->Connect();
    }
    else if (event_id == PROTOCOL_STA_DISCONNECTED)
    {
        ws->setReady(false);
    }
}

void SigmaWS::Connect()
{
    PLogger->Internal("WS connecting");
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
    ws->PLogger->Internal("TCP connected");

    // Generate WebSocket key - random 16 bytes base64 encoded
    byte randomKey[16];
    for (int i = 0; i < 16; i++)
    {
        randomKey[i] = random(0, 256);
    }

    // Base64 encode the key
    char *buffer;
    int bufferLen = base64_encode_expected_len(16);
    buffer = (char *)malloc(bufferLen);
    base64_encode_chars((char *)randomKey, 16, buffer);
    String wsKey = String(buffer);
    free(buffer);
    ws->PLogger->Append("Generated WebSocket Key: ").Append(wsKey).Internal();

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

    ws->PLogger->Append("Sending handshake:").Append(handshake).Internal();

    // Send handshake
    wsClient.add(handshake.c_str(), handshake.length());
    if (wsClient.send())
    {
        ws->PLogger->Append("Sent WebSocket handshake").Internal();
        ws->setReady(true);
    }
    else
    {
        ws->PLogger->Append("Failed to send WebSocket handshake").Internal();
    }
    // IsReady set after first handshake

    // ws->isReady = true;
    if (ws->config.authType & (AUTH_TYPE_BASIC | AUTH_TYPE_URL | AUTH_TYPE_NONE))
    {
        ws->setReady(true);
    }
    ws->PLogger->Append("Sent WebSocket handshake").Internal();
}

void SigmaWS::setReady(bool ready)
{
    isReady = ready;
    PLogger->Append("Setting ready to ").Append(ready).Internal();
    esp_event_post_to(SigmaProtocol::GetEventLoop(), GetName().c_str(), isReady ? PROTOCOL_CONNECTED : PROTOCOL_DISCONNECTED, (void *)(GetName().c_str()), GetName().length() + 1, portMAX_DELAY);
}

void SigmaWS::onDisconnect(void *arg, AsyncClient *c)
{
    SigmaWS *ws = (SigmaWS *)arg;
    ws->setReady(false);
}

void SigmaWS::onData(void *arg, AsyncClient *c, void *data, size_t len)
{
    SigmaWS *ws = (SigmaWS *)arg;
    ws->PLogger->Append("Received data").Internal();
    char *buf = (char *)data;
    String response = String(buf, len);
    ws->PLogger->Append("Received data from WebSocket server").Internal();
    ws->PLogger->Append(response).Internal();
    ws->PLogger->Append("**************************************************").Internal();

    if (response.indexOf("HTTP/1.1 101") >= 0 &&
        response.indexOf("Upgrade: websocket") >= 0)
    {
        ws->PLogger->Append("WebSocket handshake successful").Internal();
        if (!ws->isReady && ws->config.authType & (AUTH_TYPE_FIRST_MESSAGE))
        {
            JsonDocument doc;
            doc["type"] = "auth";
            doc["apiKey"] = ws->config.apiKey;
            doc["client"] = ws->config.clientId;
            String jsonStr;
            serializeJson(doc, jsonStr);
            ws->sendWebSocketTextFrame(jsonStr);
            ws->setReady(true);
        }
        else
        {
            // Parse WebSocket frame
            ws->PLogger->Append("Received data from WebSocket server").Internal();
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

                        ws->PLogger->Append("Received: ").Append(payload).Internal();

                        if (SigmaInternalPkg::IsSigmaInternalPkg(payload))
                        {
                            SigmaInternalPkg pkg(payload);
                            esp_event_post_to(SigmaProtocol::GetEventLoop(), ws->GetName().c_str(), PROTOCOL_RECEIVED_SIGMA_MESSAGE, (void *)(&pkg), sizeof(pkg), portMAX_DELAY);
                            TopicSubscription *subscription = ws->GetSubscription(pkg.GetTopic());
                            if (subscription != nullptr)
                            {
                                esp_event_post_to(SigmaProtocol::GetEventLoop(), ws->GetName().c_str(), subscription->eventId, (void *)(&pkg), sizeof(pkg), portMAX_DELAY);
                            }
                        }
                        else
                        {
                            esp_event_post_to(SigmaProtocol::GetEventLoop(), ws->GetName().c_str(), PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE, (void *)(payload.c_str()), payload.length(), portMAX_DELAY);
                        }

                        break;
                    }
                    case 0x02:
                    { // Binary frame
                        byte *payload;
                        bool isFreeRequired = false;
                        // Unmask if needed
                        if (masked)
                        {
                            uint8_t mask[4];
                            memcpy(mask, bytes + headerLen - 4, 4);

                            payload = (byte *)malloc(payload_len);
                            isFreeRequired = true;
                            for (size_t i = 0; i < payload_len; i++)
                            {
                                payload[i] = (bytes[headerLen + i] ^ mask[i % 4]);
                            }
                        }
                        else
                        {
                            payload = bytes + headerLen;
                            // memcpy(payload, bytes + headerLen, payload_len);
                        }
                        BinaryData *bd = (BinaryData *)payload;
                        esp_event_post_to(SigmaProtocol::GetEventLoop(), ws->GetName().c_str(), PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE, (void *)(bd), sizeof(bd), portMAX_DELAY);
                        if (isFreeRequired)
                        {
                            free(payload);
                        }
                        break;
                    }
                    case 0x08:
                    {
                        // Close frame. Server is closing the connection. We will try to reconnect
                        ws->PLogger->Append("Received close frame").Internal();
                        ws->Disconnect();
                        // ws->setReady(false);
                        ws->Connect();
                        break;
                    }
                    case 0x09:
                    {
                        // Ping frame.
                        ws->PLogger->Append("Received ping frame").Internal();
                        esp_event_post_to(SigmaProtocol::GetEventLoop(), ws->GetName().c_str(), PROTOCOL_RECEIVED_PING, data, len, portMAX_DELAY);
                        // TODO: ws->SendWebSocketFramePong(""); // Pong
                        break;
                    }
                    case 0x0A:
                    {
                        // Pong frame
                        ws->PLogger->Append("Received pong frame").Internal();
                        esp_event_post_to(SigmaProtocol::GetEventLoop(), ws->GetName().c_str(), PROTOCOL_RECEIVED_PONG, data, len, portMAX_DELAY);
                        // TODO: ws->SendWebSocketFramePong(""); // Pong
                        break;
                    }
                    default:
                    {
                        // Unknown frame
                        ws->PLogger->Append("Unknown frame").Internal();
                        break;
                    }
                    }
                }
            }
            else
            {
                ws->PLogger->Append("WebSocket handshake failed").Internal();
                String error = "[" + ws->GetName() + "] " + "WebSocket handshake failed";
                esp_event_post_to(SigmaProtocol::GetEventLoop(), ws->GetName().c_str(), PROTOCOL_ERROR, (void *)(error.c_str()), error.length(), portMAX_DELAY);
            }
        }
    }
}

void SigmaWS::onError(void *arg, AsyncClient *c, int8_t error)
{
    SigmaWS *ws = (SigmaWS *)arg;
    String errorStr = "[" + ws->GetName() + "] " + "TCP error: " + error;
    ws->PLogger->Append(errorStr).Internal();
    esp_event_post_to(SigmaProtocol::GetEventLoop(), ws->GetName().c_str(), PROTOCOL_ERROR, (void *)errorStr.c_str(), errorStr.length(), portMAX_DELAY);
}

void SigmaWS::onTimeout(void *arg, AsyncClient *c, uint32_t time)
{
    SigmaWS *ws = (SigmaWS *)arg;
    String errorStr = "[" + ws->GetName() + "] " + "TCP timeout after " + String(time) + "ms";
    ws->PLogger->Append(errorStr).Internal();
    esp_event_post_to(SigmaProtocol::GetEventLoop(), ws->GetName().c_str(), PROTOCOL_ERROR, (void *)errorStr.c_str(), errorStr.length(), portMAX_DELAY);
    //    c->close();
}

void SigmaWS::sendWebSocketTextFrame(const String &payload)
{
    sendWebSocketFrame((const byte *)payload.c_str(), payload.length(), 0x01);
}

void SigmaWS::sendWebSocketBinaryFrame(const byte *data, size_t size)
{
    sendWebSocketFrame(data, size, 0x02);
}

void SigmaWS::sendWebSocketPingFrame(const String &payload)
{   
    sendWebSocketFrame((const byte *)payload.c_str(), payload.length(), 0x09);
}

void SigmaWS::sendWebSocketPongFrame(const String &payload)
{
    sendWebSocketFrame((const byte *)payload.c_str(), payload.length(), 0x0A);
}

void SigmaWS::sendWebSocketFrame(const byte *payload, size_t payloadLen, byte opcode)
{
    if (!isReady)
    {
        PLogger->Append("Cannot send message - not connected").Internal();
        return;
    }

    // Create WebSocket frame
    // Format: FIN=1, RSV1-3=0, Opcode=1(text) or 2(binary)
    // MASK=1 (client to server must be masked)
    // Payload length + mask + payload

    size_t headerSize = 2; // Basic header size
    
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

    for (size_t i = 0; i < payloadLen; i++)
    {
        buffer[headerSize + i] = payload[i] ^ maskKey[i % 4];
    }
    
    // Send frame
    wsClient.add((const char *)buffer, headerSize + payloadLen);
    wsClient.send();
    delete[] buffer;
}
