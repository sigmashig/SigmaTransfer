#include "SigmaWsClient.h"
#include <ArduinoJson.h>
#include <libb64/cencode.h>
#include "SigmaAsyncNetwork.h"
#include "SigmaInternalPkg.h"
#include <WiFi.h>
#include <esp_event.h>

// ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);
ESP_EVENT_DECLARE_BASE(SIGMAASYNCNETWORK_EVENT);

SigmaWsClient::SigmaWsClient(WSClientConfig _config, SigmaLoger *logger, uint priority) : SigmaConnection("SigmaWsClient", logger, priority)
{
    config = _config;
    this->name = name;
    // setRootTopic(config.rootTopic);
    wsClient.onConnect(onConnect, this);
    wsClient.onDisconnect(onDisconnect, this);
    wsClient.onData(onData, this);
    wsClient.onError(onError, this);
    wsClient.onTimeout(onTimeout, this);

    esp_event_handler_register_with(SigmaAsyncNetwork::GetEventLoop(), SIGMAASYNCNETWORK_EVENT, ESP_EVENT_ANY_ID, networkEventHandler, this);
    esp_event_handler_register_with(eventLoop, GetEventBase(), PROTOCOL_SEND_RAW_BINARY_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(eventLoop, GetEventBase(), PROTOCOL_SEND_RAW_TEXT_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(eventLoop, GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, protocolEventHandler, this);
    // esp_event_handler_register_with(eventLoop, ESP_EVENT_ANY_BASE, PROTOCOL_SEND_PING, protocolEventHandler, this);
    esp_event_handler_register_with(eventLoop, GetEventBase(), PROTOCOL_SEND_PONG, protocolEventHandler, this);
}

void SigmaWsClient::protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaWsClient *ws = (SigmaWsClient *)arg;
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
    else if (event_id == PROTOCOL_SEND_PONG)
    {
        String *payload = (String *)event_data;
        ws->sendWebSocketPongFrame(*payload);
    }
}

void SigmaWsClient::networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaWsClient *ws = (SigmaWsClient *)arg;
    if (event_id == PROTOCOL_STA_CONNECTED)
    {
        ws->Connect();
    }
    else if (event_id == PROTOCOL_STA_DISCONNECTED)
    {
        ws->setReady(false);
    }
}

void SigmaWsClient::Connect()
{
    Log->Internal("WS connecting");
    if (!SigmaAsyncNetwork::IsConnected())
    {
        Log->Internal("WS cannot connect - network is not connected");
        return;
    }
    String url = config.host;
    if (config.authType & AUTH_TYPE_URL)
    {
        url += "?apiKey=" + config.apiKey + "&clientId=" + config.clientId;
    }
    wsClient.connect(url.c_str(), config.port);
}

void SigmaWsClient::Disconnect()
{
    sendWebSocketCloseFrame();
    wsClient.close();
}

void SigmaWsClient::onConnect(void *arg, AsyncClient *c)
{
    SigmaWsClient *ws = (SigmaWsClient *)arg;
    ws->Log->Internal("Websocket connected");

    // Generate WebSocket key - random 16 bytes base64 encoded
    byte randomKey[16];
    for (int i = 0; i < 16; i++)
    {
        randomKey[i] = random(0, 256);
    }

    // Base64 encode the key
    char *buffer;
    int bufferLen = base64_encode_expected_len(16) + 1;
    buffer = (char *)malloc(bufferLen);
    int len = base64_encode_chars((char *)randomKey, 16, buffer);
    buffer[len] = '\0';
    String wsKey = String(buffer);
    free(buffer);

    // Create HTTP upgrade request
    String handshake = "GET ";
    handshake += ws->config.rootPath;
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

    // ws->Log->Append("Sending handshake:").Append(handshake).Internal();

    // Send handshake
    wsClient.add(handshake.c_str(), handshake.length());
    if (wsClient.send())
    {
        ws->Log->Append("Sent WebSocket handshake").Internal();
    }
    else
    {
        ws->Log->Append("Failed to send WebSocket handshake").Internal();
    }
    if (ws->config.authType == AUTH_TYPE_FIRST_MESSAGE)
    {
        ws->sendAuthMessage();
    }
    ws->setReady(true);
}

void SigmaWsClient::sendAuthMessage()
{
    JsonDocument doc;
    doc["type"] = "auth";
    doc["apiKey"] = config.apiKey;
    doc["client"] = config.clientId;
    String jsonStr;
    serializeJson(doc, jsonStr);
    sendWebSocketTextFrame(jsonStr, true);
}

void SigmaWsClient::setReady(bool ready)
{
    isReady = ready;
    Log->Append("Setting ready to ").Append(ready).Internal();
    esp_event_post_to(SigmaConnection::GetEventLoop(), GetEventBase(), isReady ? PROTOCOL_CONNECTED : PROTOCOL_DISCONNECTED, (void *)(GetName().c_str()), GetName().length() + 1, portMAX_DELAY);
}

void SigmaWsClient::onDisconnect(void *arg, AsyncClient *c)
{
    SigmaWsClient *ws = (SigmaWsClient *)arg;
    ws->setReady(false);
}

void SigmaWsClient::onData(void *arg, AsyncClient *c, void *data, size_t len)
{
    SigmaWsClient *ws = (SigmaWsClient *)arg;
    char *buf = (char *)data;
    String response = String(buf, len);
    // ws->Log->Append("Received data from WebSocket server").Internal();
    // ws->Log->Append(response).Internal();
    // ws->Log->Append("**************************************************").Internal();

    if (response.indexOf("HTTP/1.1 101") >= 0 &&
        response.indexOf("Upgrade: websocket") >= 0)
    {
        // Parse WebSocket frame
        ws->Log->Append("Received data from WebSocket server").Internal();
        byte *bytes = (byte *)data;
        byte opcode = bytes[0] & 0x0F;
        bool masked = (bytes[1] & 0x80) != 0;
        ulong payload_len = bytes[1] & 0x7F;
        ws->Log->Append("Opcode: ").Append(opcode).Internal();
        ws->Log->Append("Masked: ").Append(masked).Internal();
        ws->Log->Append("Payload length: ").Append(payload_len).Internal();

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
                String payload;
                if (opcode != 0x02)
                {
                    ws->Log->Append("Received text frame").Internal();
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
                }
                switch (opcode)
                {
                case 0x01:
                {
                    // Text frame
                    ws->Log->Append("Received: ").Append(payload).Internal();

                    if (SigmaInternalPkg::IsSigmaInternalPkg(payload))
                    {
                        SigmaInternalPkg pkg(payload);
                        esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_RECEIVED_SIGMA_MESSAGE, (void *)(&pkg), sizeof(pkg), portMAX_DELAY);
                        TopicSubscription *subscription = ws->GetSubscription(pkg.GetTopic());
                        if (subscription != nullptr)
                        {
                            esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), subscription->eventId, (void *)(&pkg), sizeof(pkg), portMAX_DELAY);
                        }
                    }
                    else
                    {
                        esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE, (void *)(payload.c_str()), payload.length() + 1, portMAX_DELAY);
                    }
                    break;
                }
                case 0x02:
                { // Binary frame
                    byte *bPayload;
                    bool isFreeRequired = false;
                    // Unmask if needed
                    if (masked)
                    {
                        uint8_t mask[4];
                        memcpy(mask, bytes + headerLen - 4, 4);

                        bPayload = (byte *)malloc(payload_len);
                        isFreeRequired = true;
                        for (size_t i = 0; i < payload_len; i++)
                        {
                            bPayload[i] = (bytes[headerLen + i] ^ mask[i % 4]);
                        }
                    }
                    else
                    {
                        bPayload = bytes + headerLen;
                    }
                    BinaryData *bd = (BinaryData *)bPayload;
                    esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE, (void *)(bd), sizeof(bd), portMAX_DELAY);
                    if (isFreeRequired)
                    {
                        free(bPayload);
                    }
                    break;
                }
                case 0x08:
                {
                    // Close frame. Server is closing the connection. We will try to reconnect
                    ws->Log->Append("Received close frame").Internal();
                    ws->Disconnect();
                    // ws->setReady(false);
                    if (ws->shouldConnect)
                    {
                        ws->Connect();
                    }
                    break;
                }
                case 0x09:
                {
                    // Ping frame.
                    ws->Log->Append("Received ping frame").Internal();
                    esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_RECEIVED_PING, (void *)(payload.c_str()), payload.length() + 1, portMAX_DELAY);
                    ws->sendWebSocketPongFrame(payload);
                    break;
                }
                case 0x0A:
                {
                    // Pong frame
                    ws->Log->Append("Received pong frame").Internal();
                    esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_RECEIVED_PONG, (void *)(payload.c_str()), payload.length() + 1, portMAX_DELAY);
                    break;
                }
                default:
                {
                    // Unknown frame
                    ws->Log->Append("Unknown frame").Internal();
                    break;
                }
                }
            }
        }
        else
        {
            ws->Log->Append("WebSocket handshake failed").Internal();
            String error = "[" + ws->GetName() + "] " + "WebSocket handshake failed";
            esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_ERROR, (void *)(error.c_str()), error.length() + 1, portMAX_DELAY);
        }
    }
}

void SigmaWsClient::onError(void *arg, AsyncClient *c, int8_t error)
{
    SigmaWsClient *ws = (SigmaWsClient *)arg;
    String errorStr = "[" + ws->GetName() + "] " + "TCP error: " + error;
    ws->Log->Append(errorStr).Internal();
    esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_ERROR, (void *)errorStr.c_str(), errorStr.length() + 1, portMAX_DELAY);
}

void SigmaWsClient::onTimeout(void *arg, AsyncClient *c, uint32_t time)
{
    SigmaWsClient *ws = (SigmaWsClient *)arg;
    String errorStr = "[" + ws->GetName() + "] " + "TCP timeout after " + String(time) + "ms";
    ws->Log->Append(errorStr).Internal();
    esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_ERROR, (void *)errorStr.c_str(), errorStr.length() + 1, portMAX_DELAY);
    //    c->close();
}

bool SigmaWsClient::sendWebSocketTextFrame(const String &payload, bool isAuth)
{
    return sendWebSocketFrame((const byte *)payload.c_str(), payload.length(), 0x01, isAuth);
}

bool SigmaWsClient::sendWebSocketBinaryFrame(const byte *data, size_t size)
{
    return sendWebSocketFrame(data, size, 0x02);
}

bool SigmaWsClient::sendWebSocketPingFrame(const String &payload)
{
    return sendWebSocketFrame((const byte *)payload.c_str(), payload.length(), 0x09);
}

bool SigmaWsClient::sendWebSocketPongFrame(const String &payload)
{
    return sendWebSocketFrame((const byte *)payload.c_str(), payload.length(), 0x0A);
}

bool SigmaWsClient::sendWebSocketCloseFrame()
{
    return sendWebSocketFrame(nullptr, 0, 0x08);
}

bool SigmaWsClient::sendWebSocketFrame(const byte *_payload, size_t _payloadLen, byte opcode, bool isAuth)
{
    if (!isReady && !isAuth)
    {
        Log->Append("Cannot send message - not connected").Internal();
        return false;
    }

    byte *payload = (byte *)_payload;
    size_t payloadLen = _payloadLen;
    String jsonStr;

    if (opcode == 0x01 && config.authType & AUTH_TYPE_ALL_MESSAGES)
    {
        // Convert to Json
        // Text frame

        JsonDocument doc;

        doc["apiKey"] = config.apiKey;
        doc["client"] = config.clientId;
        doc["data"] = payload;
        serializeJson(doc, jsonStr);

        payload = (byte *)jsonStr.c_str();
        payloadLen = jsonStr.length();
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
    delete[] buffer;
    return wsClient.send();
}
