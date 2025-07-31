#include "SigmaWsClient.h"
#include <ArduinoJson.h>
#include <libb64/cencode.h>
#include "SigmaAsyncNetwork.h"
#include "SigmaInternalPkg.h"
#include <WiFi.h>
#include <esp_event.h>

SigmaWsClient::SigmaWsClient(WSClientConfig _config, SigmaLoger *logger, uint priority) : SigmaConnection("SigmaWsClient", logger, priority)
{
    config = _config;
    retryConnectingCount = config.retryConnectingCount;
    retryConnectingDelay = config.retryConnectingDelay;
    pingInterval = config.pingInterval;
    pingRetryCount = config.pingRetryCount;

    wsClientConfig = {};
    wsClientConfig.uri = config.host.c_str();
    wsClientConfig.port = config.port;
    wsClientConfig.path = config.rootPath.c_str();
    wsClientConfig.disable_auto_reconnect = false;
    wsClientConfig.task_prio = 1;
    wsClientConfig.task_stack = 4096;
    wsClientConfig.buffer_size = 1024;
    if (config.pingType == PING_BINARY)
    {
        wsClientConfig.ping_interval_sec = config.pingInterval;
        wsClientConfig.pingpong_timeout_sec = 20;
        wsClientConfig.disable_pingpong_discon = true;
    }
    else
    {
        wsClientConfig.ping_interval_sec = 0;
        wsClientConfig.pingpong_timeout_sec = 0;
        wsClientConfig.disable_pingpong_discon = false;
    }
    wsClientConfig.keep_alive_enable = true;

    if (config.authType & AUTH_TYPE_BASIC)
    {
        Log->Append("Using Basic Authentication").Internal();
        String authRec = "Authorization: Basic \r\n";
        authRec += "Bearer:" + config.apiKey + "\r\n";
        /*
        char *buffer;
        int bufferLen = base64_encode_expected_len(config.apiKey.length());
        buffer = (char *)malloc(bufferLen);
        base64_encode_chars((char *)(config.apiKey.c_str()), config.apiKey.length(), buffer);
        authRec += String(buffer);
        free(buffer);
        // authRec += "\r\n"; Should be checked
        */
        wsClientConfig.headers = authRec.c_str();
        Log->Append("Headers: ").Append(authRec).Internal();
    }

    wsClient = esp_websocket_client_init(&wsClientConfig);
    if (wsClient == NULL)
    {
        Log->Append("Failed to initialize WebSocket client").Internal();
        return;
    }

    esp_err_t err = esp_websocket_register_events(wsClient, WEBSOCKET_EVENT_ANY, wsEventHandler, this);

    if (err != ESP_OK)
    {
        Log->Append("Failed to register WebSocket events: ").Append(esp_err_to_name(err)).Internal();
        return;
    }
    /*
        err = esp_websocket_client_start(wsClient);
        if (err != ESP_OK)
        {
            Log->Append("Failed to start WebSocket client: ").Append(esp_err_to_name(err)).Internal();
        }
    */
    err = esp_event_handler_register_with(SigmaAsyncNetwork::GetEventLoop(), SigmaAsyncNetwork::GetEventBase(), ESP_EVENT_ANY_ID, networkEventHandler, this);
    if (err != ESP_OK)
    {
        Log->Append("Failed to register network event handler: ").Append(esp_err_to_name(err)).Internal();
    }

    err = esp_event_handler_register_with(eventLoop, GetEventBase(), PROTOCOL_SEND_RAW_BINARY_MESSAGE, protocolEventHandler, this);
    if (err != ESP_OK)
    {
        Log->Append("Failed to register protocol[PROTOCOL_SEND_RAW_BINARY_MESSAGE] event handler: ").Append(esp_err_to_name(err)).Internal();
    }
    err = esp_event_handler_register_with(eventLoop, GetEventBase(), PROTOCOL_SEND_RAW_TEXT_MESSAGE, protocolEventHandler, this);
    if (err != ESP_OK)
    {
        Log->Append("Failed to register protocol[PROTOCOL_SEND_RAW_TEXT_MESSAGE] event handler: ").Append(esp_err_to_name(err)).Internal();
    }

    setPingTimer(this);
    // pingTimer = xTimerCreate("PingTimer", pdMS_TO_TICKS(config.pingInterval*1000), pdTRUE, this, pingTask);
    // Log->Append("[SigmaWsClient]PingTimer created:").Append(config.pingInterval).Internal();
    // if (pingTimer == NULL)
    //{
    //     Log->Append("Failed to create ping timer").Internal();
    // }
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
        SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
        ws->sendWebSocketTextFrame(pkg.GetPkgString());
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
    else if (event_id == PROTOCOL_SEND_RECONNECT)
    {
        ws->Disconnect();
    }
    else if (event_id == PROTOCOL_SEND_CLOSE)
    {
        ws->Close();
    }
}

void SigmaWsClient::networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaWsClient *ws = (SigmaWsClient *)arg;
    if (event_id == PROTOCOL_STA_CONNECTED)
    {
        ws->Log->Append("[networkEventHandler]Network connected").Internal();
        ws->Connect();
        ws->clearReconnectTimer(ws);
    }
    else if (event_id == PROTOCOL_STA_DISCONNECTED)
    {
        ws->setReady(false);
    }
}

void SigmaWsClient::wsEventHandler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    SigmaWsClient *ws = (SigmaWsClient *)handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
    {
        ws->Log->Append("[wsEventHandler] WebSocket Connected").Internal();
        ws->onConnect(*data, ws);
        break;
    }
    case WEBSOCKET_EVENT_DISCONNECTED:
    {
        ws->Log->Append("[wsEventHandler] WebSocket Disconnected").Internal();
        ws->onDisconnect(*data, ws);
        break;
    }
    case WEBSOCKET_EVENT_DATA:
    {
        ws->Log->Append("[wsEventHandler] WebSocket Data Received").Internal();
        ws->onData(*data, ws);
        break;
    }
    case WEBSOCKET_EVENT_ERROR:
    {
        ws->Log->Append("[wsEventHandler] WebSocket Error").Internal();
        ws->onError(*data, ws);
        break;
    }

    case WEBSOCKET_EVENT_CLOSED:
    {
        ws->Log->Append("[wsEventHandler] WebSocket Closed").Internal();
        ws->setReady(false);
        ws->Disconnect(); // This will handle reconnection logic
        break;
    }

    default:
        ws->Log->Append("[wsEventHandler] Unknown WebSocket Event: ").Append(event_id).Internal();
        break;
    }
}

void SigmaWsClient::Connect()
{
    if (!SigmaAsyncNetwork::IsConnected())
    {
        Log->Internal("WS cannot connect - network is not connected");
        return;
    }
    String url = config.host;
    if (config.authType & AUTH_TYPE_URL)
    {
        url = String((url.endsWith("/") ? "" : "/")) + "?apiKey=" + config.apiKey + "&clientId=" + config.clientId;
        esp_websocket_client_set_uri(wsClient, url.c_str());
    }
    Log->Append("Connecting to: ").Append(url).Append(":").Append(config.port).Internal();

    esp_err_t err = esp_websocket_client_start(wsClient);

    if (err != ESP_OK)
    {
        Log->Append("[Connect]Failed to start WebSocket client: ").Append(esp_err_to_name(err)).Error();
    }

    retryConnectingCount = config.retryConnectingCount;
    pingRetryCount = config.pingRetryCount;
    setReconnectTimer(this);
}

void SigmaWsClient::Disconnect()
{
    sendWebSocketCloseFrame();
    esp_websocket_client_stop(wsClient);
    if (retryConnectingCount <= 0)
    {
        Log->Append("Failed to connect to WebSocket server:").Append(retryConnectingCount).Internal();
        return;
    }
    if (shouldConnect)
    {
        Log->Append("Retrying to connect to WebSocket server:").Append(retryConnectingCount).Internal();
        setReconnectTimer(this);
    }
    else
    {
        clearPingTimer(this);
    }
}

void SigmaWsClient::sendPing()
{
    if (pingRetryCount <= 0)
    {
        Disconnect();
        esp_event_post_to(GetEventLoop(), GetEventBase(), PROTOCOL_PING_TIMEOUT, (void *)(GetName().c_str()), GetName().length() + 1, portMAX_DELAY);
    }
    else
    {
        Log->Append("[sendPing]Sending PING frame").Internal();
        pingRetryCount--;
        sendWebSocketPingFrame("PING");
    }
}

void SigmaWsClient::onConnect(esp_websocket_event_data_t &arg, SigmaWsClient *ws)
{
    /*
    esp_err_t err = esp_websocket_client_start(ws->wsClient);
    if (err != ESP_OK)
    {
        ws->Log->Append("[onConnect]Failed to start WebSocket client: ").Append(esp_err_to_name(err)).Internal();
    }
*/
    ws->clearReconnectTimer(ws);
    ws->retryConnectingCount = ws->config.retryConnectingCount;
    ws->pingRetryCount = ws->config.pingRetryCount;
    ws->setReady(true);
    if (ws->config.authType & AUTH_TYPE_FIRST_MESSAGE)
    {
        ws->sendAuthMessage();
    }
    // ws->setReconnectTimer(ws);
}

void SigmaWsClient::sendAuthMessage()
{
    JsonDocument doc;
    doc["type"] = "auth";
    doc["authKey"] = config.apiKey;
    doc["clientId"] = config.clientId;
    doc["pingType"] = config.pingType == PING_TEXT ? "PING_TEXT" : config.pingType == PING_BINARY ? "PING_BINARY"
                                                                                                  : "PING_NO";
    String jsonStr;
    serializeJson(doc, jsonStr);
    sendWebSocketTextFrame(jsonStr, true);
}

void SigmaWsClient::setReady(bool ready)
{
    isReady = ready;
    Log->Append("Setting ready: ").Append(ready).Internal();
    clearReconnectTimer(this);
    if (ready)
    {
        retryConnectingCount = config.retryConnectingCount;
    }
    esp_event_post_to(SigmaConnection::GetEventLoop(), GetEventBase(), isReady ? PROTOCOL_CONNECTED : PROTOCOL_DISCONNECTED, (void *)(GetName().c_str()), GetName().length() + 1, portMAX_DELAY);
}

void SigmaWsClient::onDisconnect(esp_websocket_event_data_t &arg, SigmaWsClient *ws)
{
    ws->clearReconnectTimer(ws);
    ws->Log->Append("[ondisconnect]Disconnected from WebSocket server").Internal();
    ws->setReady(false);
    if (ws->shouldConnect)
    {
        ws->Disconnect(); // This will handle reconnection logic
    }
}

void SigmaWsClient::onDataText(String &payload, SigmaWsClient *ws)
{
    ws->pingRetryCount = ws->config.pingRetryCount;
    String UpperPayload = payload;
    UpperPayload.toUpperCase();
    if (UpperPayload.startsWith("PING"))
    {
        ws->Log->Append("Received PING frame").Internal();
        esp_websocket_client_send_text(ws->wsClient, "PONG", 4, portMAX_DELAY);
        esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_RECEIVED_PING,
                          (void *)(payload.c_str()), payload.length() + 1, portMAX_DELAY);
    }
    else if (UpperPayload.startsWith("PONG"))
    {
        ws->Log->Append("Received textPONG frame").Internal();
        esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_RECEIVED_PONG,
                          (void *)(payload.c_str()), payload.length() + 1, portMAX_DELAY);
    }
    else if (UpperPayload.startsWith("CLOSE"))
    {
        ws->Log->Append("Received CLOSE frame").Internal();
        ws->Disconnect();
    }
    else if (SigmaInternalPkg::IsSigmaInternalPkg(payload))
    {
        ws->Log->Append("Received SigmaInternalPkg").Internal();
        SigmaInternalPkg pkg(payload);
        esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_RECEIVED_SIGMA_MESSAGE,
                          (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
        TopicSubscription *subscription = ws->GetSubscription(pkg.GetTopic());
        if (subscription != nullptr)
        {
            esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), subscription->eventId,
                              (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
        }
    }
    else
    {
        esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE,
                          (void *)(payload.c_str()), payload.length() + 1, portMAX_DELAY);
    }
}

void SigmaWsClient::onDataBinary(byte *payload, size_t payloadLen, SigmaWsClient *ws)
{
    ws->Log->Append("Received Binary Data").Internal();
    esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE,
                      (void *)(payload), payloadLen, portMAX_DELAY);
}

void SigmaWsClient::onData(esp_websocket_event_data_t &arg, SigmaWsClient *ws)
{
    ws->clearReconnectTimer(ws);
    ws->pingRetryCount = ws->config.pingRetryCount;
    switch (arg.op_code)
    {
    case WS_TEXT:
    {
        String response = String((char *)arg.data_ptr, arg.data_len);
        ws->Log->Append("Received data from WebSocket server:#").Append(arg.data_len).Append("#").Append(response).Internal();
        ws->onDataText(response, ws);
        break;
    }
    case WS_BINARY:
    {
        ws->Log->Append("Received Binary Data").Internal();
        ws->onDataBinary((byte *)arg.data_ptr, arg.data_len, ws);
        break;
    }
    case WS_DISCONNECT:
    {
        ws->Log->Append("Received CLOSE frame").Internal();
        // ws->setReady(false);
        ws->Disconnect();
        break;
    }
    case WS_PING:
    {
        ws->Log->Append("Received PING(binary) frame").Internal();
        break;
    }
    case WS_PONG:
    {
        ws->Log->Append("Received PONG frame").Internal();
        ws->pingRetryCount = ws->config.pingRetryCount;
        break;
    }
    }
}

void SigmaWsClient::onError(esp_websocket_event_data_t &arg, SigmaWsClient *ws)
{
    String errorStr = "[" + ws->GetName() + "] " + "WebSocket error: ";
    ws->Log->Append(errorStr).Internal();
    esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_ERROR, (void *)errorStr.c_str(), errorStr.length() + 1, portMAX_DELAY);
}
/*
void SigmaWsClient::onTimeout(void *arg, AsyncClient *c, uint32_t time)
{
    SigmaWsClient *ws = (SigmaWsClient *)arg;
    String errorStr = "[" + ws->GetName() + "] " + "TCP timeout after " + String(time) + "ms";
    ws->Log->Append(errorStr).Internal();
    esp_event_post_to(ws->GetEventLoop(), ws->GetEventBase(), PROTOCOL_ERROR, (void *)errorStr.c_str(), errorStr.length() + 1, portMAX_DELAY);
    //    c->close();
}
*/

bool SigmaWsClient::sendWebSocketTextFrame(const String &payload, bool isAuth)
{
    if (payload.length() == 0)
    {
        Log->Append("No payload to send").Internal();
        return false;
    }
    else
    {
        Log->Append("[WS:C->S]:").Append(payload).Internal();
    }
    // int payloadLen = payload.length();
    String message = payload;
    if (config.authType & AUTH_TYPE_ALL_MESSAGES)
    {
        // Convert to Json
        // Text frame

        JsonDocument doc;

        doc["apiKey"] = config.apiKey;
        doc["client"] = config.clientId;
        doc["data"] = payload;
        serializeJson(doc, message);
    }

    return 0 <= esp_websocket_client_send_text(wsClient, message.c_str(), message.length(), portMAX_DELAY) == ESP_OK;
    // return sendWebSocketFrame((const byte *)payload.c_str(), payload.length(), WS_TEXT, isAuth);
}

bool SigmaWsClient::sendWebSocketBinaryFrame(const byte *data, size_t size)
{
    return 0 <= esp_websocket_client_send_bin(wsClient, (const char *)data, size, portMAX_DELAY) == ESP_OK;
    // return sendWebSocketFrame(data, size, WS_BINARY);
}

bool SigmaWsClient::sendWebSocketPingFrame(const String &payload)
{
    if (config.pingType == PING_TEXT)
    {
        return sendWebSocketTextFrame("PING:" + payload, true);
    }
    return true;
    // return sendWebSocketFrame((const byte *)payload.c_str(), payload.length(), WS_PING);
}

bool SigmaWsClient::sendWebSocketPongFrame(const String &payload)
{
    if (config.pingType == PING_TEXT)
    {
        return sendWebSocketTextFrame("PONG:" + payload, true);
    }
    return true;
    // return sendWebSocketFrame((const byte *)payload.c_str(), payload.length(), WS_PONG);
}

bool SigmaWsClient::sendWebSocketCloseFrame()
{
    return esp_websocket_client_close(wsClient, portMAX_DELAY) == ESP_OK;
    // return sendWebSocketFrame(nullptr, 0, WS_DISCONNECT);
}

/*
bool SigmaWsClient::sendWebSocketFrame(const byte *_payload, size_t _payloadLen, byte opcode, bool isAuth)
{
    if (!isReady && !isAuth)
    {
        Log->Append("Cannot send message - not connected").Error();
        return false;
    }
    byte *payload = (byte *)_payload;
    size_t payloadLen = _payloadLen;
    String jsonStr;

    if (opcode == WS_TEXT && config.authType & AUTH_TYPE_ALL_MESSAGES)
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
    // Log->Printf("hsize=%d,buffer[0]:0x%02x", headerSize, buffer[0]).Debug();

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
    // Log->Printf("Send Frame[%d]", headerSize + payloadLen).Debug();
    // Log->Printf("#%s#", (const char *)(buffer + headerSize)).Debug();
    wsClient.add((const char *)buffer, headerSize + payloadLen);
    delete[] buffer;
    return wsClient.send();
}
    */
