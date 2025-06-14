#include "SigmaWsServer.h"
#include "SigmaInternalPkg.h"
#include "SigmaAsyncNetwork.h"
#include "SigmaProtocol.h"
#include <esp_event.h>
#include <ArduinoJson.h>

SigmaWsServer::SigmaWsServer(WSServerConfig config, SigmaLoger *logger, int priority) : SigmaProtocol("SigmaWsServer", logger, priority)
{
    this->config = config;
    this->name = name;
    isReady = false;

    xQueue = xQueueCreate(10, sizeof(SigmaWsServerData));
    xTaskCreate(processData, "ProcessData", 4096, this, 1, NULL);

    esp_event_handler_register_with(SigmaAsyncNetwork::GetEventLoop(), SIGMAASYNCNETWORK_EVENT, ESP_EVENT_ANY_ID, networkEventHandler, this);
    // esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_RAW_BINARY_MESSAGE, protocolEventHandler, this);
    // esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_RAW_TEXT_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_PING, protocolEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_PONG, protocolEventHandler, this);

    server = new AsyncWebServer(config.port);
    ws = new AsyncWebSocket(config.rootPath);
    ws->onEvent(onWsEvent);
    clients.clear();
    server->addHandler(ws);
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { request->send(200, "text/plain", "Hello, World! Use " + this->config.rootPath + " to connect via websocket"); });
    //    server->begin();
    //    setReady(true);
}

SigmaWsServer::~SigmaWsServer()
{
    delete server;
    delete ws;
}

void SigmaWsServer::Connect()
{
    if (!shouldConnect)
    {
        return;
    }
    server->begin();
    setReady(true);
    Log->Append("Websocket server started").Internal();
}

void SigmaWsServer::Disconnect()
{
    server->end();
    setReady(false);
}

void SigmaWsServer::Close()
{
    shouldConnect = false;
    Disconnect();
}

void SigmaWsServer::networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaWsServer *ws = (SigmaWsServer *)arg;
    if (event_id == PROTOCOL_STA_CONNECTED)
    {
        ws->Connect();
    }
    else if (event_id == PROTOCOL_STA_DISCONNECTED)
    {
        ws->setReady(false);
    }
}

bool SigmaWsServer::sendMessageToClient(String clientId, String message)
{

    const ClientAuth *auth = GetClientAuth(clientId);
    if (auth == nullptr || auth->isAuth == false)
    {
        return false;
    }
    return auth->wsClient->text(message);
}

bool SigmaWsServer::sendMessageToClient(String clientId, byte *data, size_t size)
{
    const ClientAuth *auth = GetClientAuth(clientId);
    if (auth == nullptr || auth->isAuth == false)
    {
        return false;
    }
    AsyncWebSocketMessageBuffer *buffer = new AsyncWebSocketMessageBuffer(data, size);

    return auth->wsClient->binary(buffer);
}

bool SigmaWsServer::sendPingToClient(String clientId, String payload)
{
    const ClientAuth *auth = GetClientAuth(clientId);
    if (auth == nullptr || auth->isAuth == false)
    {
        return false;
    }
    return auth->wsClient->ping((byte *)(payload.c_str()), payload.length());
}

bool SigmaWsServer::sendPongToClient(String clientId, String payload)
{
    // Webserver doesn't support pong
    return false;
}

void SigmaWsServer::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo frameInfo;
    memcpy(&frameInfo, arg, sizeof(AwsFrameInfo));
    byte *payload = (byte *)malloc(len + 1); // 1 byte for null terminator
    memcpy(payload, data, len);
    payload[len] = '\0'; // null terminator even if it's not a string

    SigmaWsServerData message;
    message.wsServer = server;
    message.wsClient = client;
    message.frameInfo = &frameInfo;
    message.data = payload;
    message.len = len;
    message.type = type;
    xQueueSend(xQueue, &message, portMAX_DELAY);
}

void SigmaWsServer::processData(void *arg)
{
    SigmaWsServer *server = (SigmaWsServer *)arg;

    while (true)
    {
        SigmaWsServerData data;
        if (xQueueReceive(server->xQueue, &data, portMAX_DELAY) == pdPASS)
        {
            switch (data.type)
            {
            case WS_EVT_CONNECT:
            {
                server->Log->Printf("WebSocket client #%u connected from %s\n", data.wsClient->id(), data.wsClient->remoteIP().toString().c_str()).Internal();
                Serial.printf("WebSocket client #%u connected from %s\n", data.wsClient->id(), data.wsClient->remoteIP().toString().c_str());
                if (server->isClientLimitReached(server, data.wsClient))
                {
                    return;
                }

                ClientAuth auth;
                auth.clientId = "";
                auth.clientIdInt = data.wsClient->id();
                auth.isAuth = false;
                auth.wsClient = data.wsClient;
                clients[auth.clientIdInt] = auth;

                if (server->config.authType == AUTH_TYPE_URL)
                {
                    String url = data.wsServer->url();
                    int i = url.indexOf("?");
                    if (i != -1)
                    {
                        String params = url.substring(i + 1);
                        String clientId = params.substring(params.indexOf("clientId=") + 9, params.indexOf("&"));
                        String authKey = params.substring(params.indexOf("apiKey=") + 7);
                        if (server->isClientAvailable(clientId, authKey))
                        {
                            auth.clientId = clientId;
                            auth.isAuth = true;
                            if (server->isConnectionLimitReached(clientId, server, data.wsClient))
                            {
                                return;
                            }
                        }
                        else
                        {
                            server->Log->Printf("Client %s not found or auth key does not match\n", clientId.c_str()).Error();
                            data.wsClient->close();
                            server->clients.erase(data.wsClient->id());
                            // return;
                        }
                    }
                    else
                    {
                        server->Log->Error("No auth key found in url\n");
                        data.wsClient->close();
                        server->clients.erase(data.wsClient->id());
                        // return;
                    }
                }
                break;
            }
            case WS_EVT_DISCONNECT:
            {
                server->Log->Printf("WebSocket client #%u disconnected\n", data.wsClient->id()).Internal();
                server->clients.erase(data.wsClient->id());
                break;
            }
            case WS_EVT_DATA:
            {
                handleWebSocketMessage(server, data);
                break;
            }
            case WS_EVT_PONG:
            case WS_EVT_PING:
            case WS_EVT_ERROR:
            {
                break;
            }
            }
            free(data.data);
        }
    }
}

void SigmaWsServer::handleWebSocketMessage(SigmaWsServer *server, SigmaWsServerData data)
{
    ClientAuth *auth = &(server->clients[data.wsClient->id()]);
    server->Log->Append("Auth:").Append(auth->clientId).Append("#").Append(auth->isAuth).Internal();
    if (data.frameInfo->opcode == WS_BINARY)
    {
        if (auth->isAuth || server->config.authType == AUTH_TYPE_NONE)
        { // Binary format is available for authenticated clients (URL/FIRST MESSAGE) or no auth
            // Client MUST BE AUTHENTICATED before sending binary data
            // there is no check of auth attribute for AUTH_TYPE_ALL_MESSAGES
            SigmaInternalPkg pkg("", data.data, data.len, true, auth->clientId);
            esp_event_post_to(server->GetEventLoop(), server->GetEventBase(), PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length()+1, portMAX_DELAY);
        }
        else
        {
            server->Log->Printf("Client %s is not authenticated\n", auth->clientId.c_str()).Error();
            data.wsClient->close();
            server->clients.erase(data.wsClient->id());
        }
    }
    else if (data.frameInfo->opcode == WS_TEXT)
    {
        String payload = String((char *)data.data);
        server->Log->Append("Payload: ").Append(payload).Internal();
        if ((!auth->isAuth && server->config.authType == AUTH_TYPE_FIRST_MESSAGE) || server->config.authType == AUTH_TYPE_ALL_MESSAGES)
        {
            String clientId = "";
            if (server->clientAuthRequest(payload, clientId))
            {
                auth->isAuth = true;
                auth->clientIdInt = data.wsClient->id();
                auth->clientId = clientId;
                if (server->isConnectionLimitReached(auth->clientId, server, data.wsClient))
                {
                    return;
                }
            }
            else
            {
                server->Log->Printf("Client %s is not authenticated", auth->clientId.c_str()).Error();
                data.wsClient->close();
                server->clients.erase(data.wsClient->id());
                return;
            }
        }

        if (SigmaInternalPkg::IsSigmaInternalPkg(payload))
        {
            SigmaInternalPkg pkg(payload);
            server->Log->Append("Sending event to protocol(sigma):").Append(pkg.GetTopic()).Internal();
            server->Log->Append("PKG:").Append(pkg.GetTopic()).Append("#").Append(pkg.GetPayload()).Append("#").Append(pkg.GetPkgString()).Append("#").Internal();
            esp_err_t espErr = esp_event_post_to(server->GetEventLoop(), server->GetEventBase(), PROTOCOL_RECEIVED_SIGMA_MESSAGE, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
            if (espErr != ESP_OK)
            {
                server->Log->Printf("Failed to send event to protocol: %d", espErr).Error();
            }
            auto subscription = server->subscriptions.find(pkg.GetTopic());
            if (subscription != server->subscriptions.end())
            {
                server->Log->Append("Sending event to subscription:").Append(server->name).Append("#").Append(subscription->second.eventId).Internal();
                esp_event_post_to(server->GetEventLoop(), server->GetEventBase(), subscription->second.eventId, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
            }
        }
        else
        {
            server->Log->Append("Sending RAW event:").Append(payload).Internal();
            esp_err_t espErr = esp_event_post_to(server->GetEventLoop(), server->GetEventBase(), PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE, (void *)(payload.c_str()), payload.length() + 1, portMAX_DELAY);
            if (espErr != ESP_OK)
            {
                server->Log->Printf("Failed to send event to protocol: %d", espErr).Error();
            }
        }
    }
    else
    {
        server->Log->Printf("Unknown opcode: %d\n", data.frameInfo->opcode).Error();
        data.wsClient->close();
        server->clients.erase(data.wsClient->id());
    }
}

void SigmaWsServer::protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaWsServer *ws = (SigmaWsServer *)arg;
    SigmaInternalPkg *pkg = (SigmaInternalPkg *)event_data;
    if (event_id == PROTOCOL_SEND_RAW_BINARY_MESSAGE)
    {
        ws->Log->Append("Raw Binary Message Not supported for WebSocketServer!").Error();
    }
    else if (event_id == PROTOCOL_SEND_RAW_TEXT_MESSAGE)
    {
        ws->Log->Append("Raw Text Message Not supported for WebSocketServer!").Error();
    }
    else if (event_id == PROTOCOL_SEND_SIGMA_MESSAGE)
    {
        bool res;
        if (pkg->IsBinary())
        {
            res = ws->sendMessageToClient(pkg->GetClientId(), pkg->GetBinaryPayload(), pkg->GetBinaryPayloadLength());
        }
        else
        {
            res = ws->sendMessageToClient(pkg->GetClientId(), pkg->GetPayload());
        }
        if (!res)
        {
            ws->Log->Append("Failed to send message to client: ").Append(pkg->GetClientId()).Error();
        }
    }
    else if (event_id == PROTOCOL_SEND_PING)
    {
        ws->sendPingToClient(pkg->GetClientId(), pkg->GetPayload());
    }
    else if (event_id == PROTOCOL_SEND_PONG)
    {
        ws->sendPongToClient(pkg->GetClientId(), pkg->GetPayload());
    }
}

bool SigmaWsServer::clientAuthRequest(String payload, String &clientId)
{
    // Log->Append("Client auth request:").Append(payload).Internal();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Log->Append("Failed to deserialize JSON: ").Append(error.c_str()).Error();
        return false;
    }
    String cId = doc["clientId"].as<String>();
    String authKey = doc["authKey"].as<String>();
    // Log->Append("Client ID:").Append(cId).Internal();
    // Log->Append("Auth Key:").Append(authKey).Internal();
    if (isClientAvailable(cId, authKey))
    {
        clientId = cId;
        return true;
    }
    return false;
}

bool SigmaWsServer::isClientAvailable(String clientId, String authKey)
{
    // Log->Append("Checking if client is available:").Append(clientId).Append("#").Append(authKey).Append("#").Internal();
    if (allowableClients.find(clientId) != allowableClients.end())
    {
        if (allowableClients[clientId].authKey == authKey)
        {
            Log->Append("Client authenticated").Internal();
            return true;
        }
        else
        {
            Log->Append("Client not authenticated").Internal();
            return false;
        }
    }
    Log->Append("Client not found").Internal();
    return false;
}

bool SigmaWsServer::isConnectionLimitReached(String clientId, SigmaWsServer *server, AsyncWebSocketClient *client)
{
    uint n = 0;

    for (auto it = server->clients.begin(); it != server->clients.end(); it++)
    {
        if (it->second.clientId == clientId)
        {
            n++;
            if (n > server->config.maxConnectionsPerClient)
            {
                server->Log->Printf("Client %s already connected. The previous connection is closed.\n", clientId.c_str()).Error();
                client->close();
                server->clients.erase(it->second.clientIdInt);
                return true;
            }
        }
    }
    return false;
}

bool SigmaWsServer::isClientLimitReached(SigmaWsServer *server, AsyncWebSocketClient *client)
{

    if (server->clients.size() > server->config.maxClients)
    {
        server->Log->Printf("Max clients reached").Error();
        client->close();
        server->clients.erase(client->id());
        return true;
    }
    return false;
}