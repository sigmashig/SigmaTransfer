#include "SigmaWsServer.h"
#include "SigmaInternalPkg.h"
#include "SigmaAsyncNetwork.h"
#include <esp_event.h>
#include <ArduinoJson.h>
#include <vector>

SigmaWsServer::SigmaWsServer(WSServerConfig config, SigmaLoger *logger, int priority) : SigmaConnection("SigmaWsServer", logger, priority)
{
    this->config = config;
    isReady = false;
    pingInterval = config.pingInterval;
    retryConnectingDelay = 0; // No reconnect required
    
   
    Log->Append("SigmaWsServer: ").Append(name).Internal();
    Log->Append("AuthType:").Append(config.authType).Internal();
    Log->Append("MaxClients:").Append(config.maxClients).Internal();
    Log->Append("MaxConnectionsPerClient:").Append(config.maxConnectionsPerClient).Internal();

    xQueue = xQueueCreate(10, sizeof(SigmaWsServerData));
    xTaskCreate(processData, "ProcessData", 4096, this, 1, NULL);

    esp_event_handler_register_with(SigmaAsyncNetwork::GetEventLoop(), SigmaAsyncNetwork::GetEventBase(), ESP_EVENT_ANY_ID, networkEventHandler, this);
    // esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_RAW_BINARY_MESSAGE, protocolEventHandler, this);
    // esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_RAW_TEXT_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_PING, protocolEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_PONG, protocolEventHandler, this);

    setPingTimer(this);
    
    server = new AsyncWebServer(config.port);
    ws = new AsyncWebSocket(config.rootPath);
    allowableClients.clear();
    for (auto it = config.allowableClients.begin(); it != config.allowableClients.end(); it++)
    {

        Log->Append("AllowableClient:").Append(it->second.clientId).Append("#first:").Append(it->first).Append("#").Append(it->second.authKey).Internal();
        AddAllowableClient(it->second.clientId, it->second.authKey);
    }
    ws->onEvent(onWsEvent);
    clients.clear();
    server->addHandler(ws);
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { request->send(200, "text/plain", "Hello, World! Use " + this->config.rootPath + " to connect via websocket"); });
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
    Log->Append("Websocket server started. Waiting for clients...").Info();
}

void SigmaWsServer::Disconnect()
{
    server->end();
    setReady(false);
    esp_event_post_to(GetEventLoop(), GetEventBase(), PROTOCOL_DISCONNECTED, (void *)(GetName().c_str()), GetName().length() + 1, portMAX_DELAY);
    if (shouldConnect)
    {
        Connect();
    }
}

void SigmaWsServer::Close()
{
    Log->Append("Closing WsServer").Info();
    shouldConnect = false;
    Disconnect();
}


void SigmaWsServer::sendPing()
{
    Log->Append("PingTask").Internal();
    std::vector<int32_t> clientsToRemove;
    clientsToRemove.clear();
    for (auto it = clients.begin(); it != clients.end(); it++)
    {
        Log->Append("PingTask: client:").Append(it->second.clientId).Append("#").Append(it->second.pingType).Append("#").Append(it->second.pingRetryCount).Internal();
        if (it->second.pingType == NO_PING)
        {
            continue;
        }
        //server->Log->Append("Sending ping to client:").Append(it->second.clientId).Append("#").Append(it->second.pingRetryCount).Internal();
        //it->second.pingRetryCount--;
        bool res = false;
        if (it->second.pingType == PING_ONLY_TEXT)
        {
            res = sendMessageToClient(it->second, "PING");
        }
        else if (it->second.pingType == PING_ONLY_BINARY)
        {
            res = sendPingToClient(it->second, "PING");
        }
        if (!res)
        {
            Log->Append("Failed to send ping to client:").Append(it->second.clientId).Append("#").Append(it->second.pingRetryCount).Error();
        }
        it->second.pingRetryCount--;
        if (it->second.pingRetryCount < 0)
        {
            Log->Append("Client " + it->second.clientId + " disconnected due to ping timeout").Error();
            it->second.wsClient->close();
            clientsToRemove.push_back(it->second.clientNumber);
            String msg = "PING_TIMEOUT.  Client:" + it->second.clientId + " disconnected.";
            esp_event_post_to(GetEventLoop(), GetEventBase(), PROTOCOL_PING_TIMEOUT, (void *)(msg.c_str()), msg.length() + 1, portMAX_DELAY);
        }
    }
    for (auto it = clientsToRemove.begin(); it != clientsToRemove.end(); it++)  
    {
        clients.erase(*it);
    }
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
        // ws->setReady(false);
        ws->Disconnect();
    }
}
bool SigmaWsServer::sendMessageToClient(int32_t clientNumber, String message)
{
    return sendMessageToClient(clients[clientNumber], message);
}

bool SigmaWsServer::sendMessageToClient(ClientAuth auth, String message)
{
    return auth.wsClient->text(message);
}

bool SigmaWsServer::sendMessageToClient(String clientId, String message)
{
    Log->Append("sendMessageToClient:").Append(clientId).Append("#").Append(message).Internal();
    const ClientAuth *auth = GetClientAuth(clientId);
    if (auth == nullptr || auth->isAuth == false)
    {
        Serial.printf("sendMessageToClient: auth is nullptr or isAuth is false\n");
        return false;
    }
    return sendMessageToClient(*auth, message);
}

bool SigmaWsServer::sendMessageToClient(String clientId, byte *data, size_t size)
{
    Log->Append("sendMessageToClient(bin):").Append(clientId).Append("#").Append(size).Internal();
    const ClientAuth *auth = GetClientAuth(clientId);
    if (auth == nullptr || auth->isAuth == false)
    {
        return false;
    }
    AsyncWebSocketMessageBuffer *buffer = new AsyncWebSocketMessageBuffer(data, size);

    return auth->wsClient->binary(buffer);
}

bool SigmaWsServer::sendPingToClient(ClientAuth auth, String payload)
{
    return auth.wsClient->ping((byte *)(payload.c_str()), payload.length());
}

bool SigmaWsServer::sendPingToClient(String clientId, String payload)
{
    Log->Append("sendPingToClient:").Append(clientId).Append("#").Append(payload).Internal();
    const ClientAuth *auth = GetClientAuth(clientId);
    if (auth == nullptr || auth->isAuth == false)
    {
        Log->Append("auth is nullptr or isAuth is false").Error();
        return false;
    }
    return sendPingToClient(*auth, payload);
}

bool SigmaWsServer::sendPongToClient(ClientAuth auth, String payload)
{
    if (auth.pingType == PING_ONLY_BINARY)
    { // server doesn't support binary pong
        return false;
    }
    return auth.wsClient->text("PONG " + payload);
}

bool SigmaWsServer::sendPongToClient(String clientId, String payload)
{
    Log->Append("sendPongToClient:").Append(clientId).Append("#").Append(payload).Internal();
    const ClientAuth *auth = GetClientAuth(clientId);
    if (auth == nullptr || auth->isAuth == false)
    {
        Log->Append("auth is nullptr or isAuth is false").Error();
        return false;
    }
    return sendPongToClient(*auth, payload);
}

void SigmaWsServer::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    // for ping/pong frames arg and data are nullptr
    AwsFrameInfo frameInfo;
    if (arg != nullptr)
    {
        memcpy(&frameInfo, arg, sizeof(AwsFrameInfo));
    }
    byte *payload = nullptr;
    if (data != nullptr)
    {
        payload = (byte *)malloc(len + 1); // 1 byte for null terminator
        memcpy(payload, data, len);
        payload[len] = '\0'; // null terminator even if it's not a string
    }
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
                server->Log->Printf("WebSocket client #%u connected from %s\n", data.wsClient->id(), data.wsClient->remoteIP().toString().c_str()).Info();
                if (server->isClientLimitReached(server, data.wsClient))
                {
                    server->Log->Append("Client limit reached").Error();
                    data.wsClient->close();
                    return;
                }

                ClientAuth auth;
                auth.clientId = "";
                auth.clientNumber = data.wsClient->id();
                auth.isAuth = false;
                auth.wsClient = data.wsClient;
                auth.pingRetryCount = server->config.pingRetryCount;
                auth.pingType = PingType::PING_ONLY_BINARY;
                clients[auth.clientNumber] = auth;

                if (server->config.authType == AUTH_TYPE_URL)
                {
                    String url = data.wsServer->url();
                    int i = url.indexOf("?");
                    if (i != -1)
                    {
                        String params = url.substring(i + 1);
                        String clientId = params.substring(params.indexOf("clientId=") + String("clientId=").length(), params.indexOf("&"));
                        String authKey = params.substring(params.indexOf("apiKey=") + String("apiKey=").length());
                        if (server->isClientAvailable(clientId, authKey))
                        {
                            auth.clientId = clientId;
                            auth.isAuth = true;
                            if (server->isConnectionLimitReached(clientId, server, data.wsClient))
                            {
                                server->Log->Append("Connection limit reached in url").Error();
                                data.wsClient->close();
                                server->clients.erase(data.wsClient->id());
                                return;
                            }
                        }
                        else
                        {
                            server->Log->Printf("Client %s not found or auth key does not match\n", clientId.c_str()).Error();
                            data.wsClient->close();
                            server->clients.erase(data.wsClient->id());
                        }
                    }
                    else
                    {
                        server->Log->Error("No auth key found in url\n");
                        data.wsClient->close();
                        server->clients.erase(data.wsClient->id());
                    }
                }
                break;
            }
            case WS_EVT_DISCONNECT:
            {
                server->Log->Printf("WebSocket client #%u disconnected\n", data.wsClient->id()).Info();
                server->clients.erase(data.wsClient->id());
                break;
            }
            case WS_EVT_DATA:
            {
                handleWebSocketMessage(server, data);
                break;
            }
            case WS_EVT_PONG:
            {
                server->Log->Append("Received PONG frame from client:").Append(data.wsClient->id()).Internal();
                server->clients[data.wsClient->id()].pingRetryCount = server->config.pingRetryCount;
                break;
            }
            case WS_EVT_PING:
            {
                server->Log->Append("Received PING frame").Internal();
                if (server->clients.find(data.wsClient->id()) != server->clients.end())
                {
                    server->clients[data.wsClient->id()].pingRetryCount = server->config.pingRetryCount;
                    server->sendPongToClient(server->clients[data.wsClient->id()], "PONG");
                }
                break;
            }
            case WS_EVT_ERROR:
            {
                server->Log->Append("WebSocket error").Error();
                break;
            }
            default:
            {
                server->Log->Append("Unknown event type:").Append(data.type).Error();
                break;
            }
            }
            if (data.data != nullptr)
            {
                free(data.data);
            }
        }
    }
}

void SigmaWsServer::handleWebSocketMessage(SigmaWsServer *server, SigmaWsServerData data)
{
    ClientAuth *auth = &(server->clients[data.wsClient->id()]);
    auth->pingRetryCount = server->config.pingRetryCount;
    if (data.frameInfo->opcode == WS_BINARY)
    {
        if (auth->isAuth || server->config.authType == AUTH_TYPE_NONE)
        { // Binary format is available for authenticated clients (URL/FIRST MESSAGE) or no auth
            // Client MUST BE AUTHENTICATED before sending binary data
            // there is no check of auth attribute for AUTH_TYPE_ALL_MESSAGES
            SigmaInternalPkg pkg("", data.data, data.len, auth->clientId);
            esp_event_post_to(server->GetEventLoop(), server->GetEventBase(), PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
        }
        else
        {
            server->Log->Printf("Client %s is not authenticated\n", auth->clientId.c_str()).Error();
            data.wsClient->close();
            server->clients.erase(data.wsClient->id());
        }
    }
    else if (data.frameInfo->opcode == WS_DISCONNECT)
    {
        server->Log->Append("Received CLOSE frame").Internal();
        data.wsClient->close();
        server->clients.erase(data.wsClient->id());
    }
    else if (data.frameInfo->opcode == WS_TEXT)
    {
        String payload = String((char *)data.data);
        if (payload.startsWith("PONG") || payload.startsWith("pong"))
        {
            server->Log->Append("Received PONG frame").Internal();
            server->clients[data.wsClient->id()].pingRetryCount = server->config.pingRetryCount;
            return;
        }
        //server->Log->Append("Payload: ").Append(payload).Internal();

        if ((!auth->isAuth && server->config.authType == AUTH_TYPE_FIRST_MESSAGE) || server->config.authType == AUTH_TYPE_ALL_MESSAGES)
        {
            String clientId = "";
            server->Log->Append("AuthRequest:").Append(payload).Internal();
            if (server->clientAuthRequest(payload, clientId))
            {
                server->Log->Append("AuthRequest:OK").Internal();
                auth->isAuth = true;
                auth->clientNumber = data.wsClient->id();
                auth->clientId = clientId;
                auth->pingType = server->allowableClients[clientId].pingType;
                if (server->isConnectionLimitReached(auth->clientId, server, data.wsClient))
                {
                    server->Log->Append("Connection limit reached for client:").Append(auth->clientId).Error();
                    data.wsClient->close();
                    server->clients.erase(data.wsClient->id());
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
            //server->Log->Append("Sending event to protocol(sigma):").Append(pkg.GetTopic()).Internal();
            //server->Log->Append("PKG:").Append(pkg.GetTopic()).Append("#").Append(pkg.GetPayload()).Append("#").Append(pkg.GetPkgString()).Append("#").Internal();
            esp_err_t espErr = esp_event_post_to(server->GetEventLoop(), server->GetEventBase(), PROTOCOL_RECEIVED_SIGMA_MESSAGE, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
            if (espErr != ESP_OK)
            {
                server->Log->Printf("Failed to send event to protocol: %d", espErr).Error();
            }
            auto subscription = server->subscriptions.find(pkg.GetTopic());
            if (subscription != server->subscriptions.end())
            {
                //server->Log->Append("Sending event to subscription:").Append(server->name).Append("#").Append(subscription->second.eventId).Internal();
                esp_event_post_to(server->GetEventLoop(), server->GetEventBase(), subscription->second.eventId, (void *)(pkg.GetPkgString().c_str()), pkg.GetPkgString().length() + 1, portMAX_DELAY);
            }
        }
        else
        {
            //server->Log->Append("Sending RAW event:").Append(payload).Internal();
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
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
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
        if (pkg.IsBinary())
        {
            Serial.printf("sendMessageToClient: binary\n");
            Serial.println(pkg.GetPkgString().c_str());
            res = ws->sendMessageToClient(pkg.GetClientId(), pkg.GetBinaryPayload(), pkg.GetBinaryPayloadLength());
        }
        else
        {
            res = ws->sendMessageToClient(pkg.GetClientId(), pkg.GetPkgString());
        }
        if (!res)
        {
            ws->Log->Append("Failed to send message to client: ").Append(pkg.GetClientId()).Error();
        }
    }
    else if (event_id == PROTOCOL_SEND_PING)
    {
        ws->sendPingToClient(pkg.GetClientId(), pkg.GetPayload());
    }
    else if (event_id == PROTOCOL_SEND_PONG)
    {
        ws->sendPongToClient(pkg.GetClientId(), pkg.GetPayload());
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
    Log->Append("Client ID:").Append(cId).Internal();
    Log->Append("Auth Key:").Append(authKey).Internal();
    if (isClientAvailable(cId, authKey))
    {
        clientId = cId;
        return true;
    }
    return false;
}

bool SigmaWsServer::isClientAvailable(String clientId, String authKey)
{
    Log->Append("Checking if client is available:").Append(clientId).Append("#").Append(authKey).Append("#").Internal();
    if (allowableClients.find(clientId) != allowableClients.end())
    {
        if (allowableClients[clientId].authKey == authKey)
        {
            Log->Append("Client:").Append(clientId).Append(" allowed").Internal();
            return true;
        }
        else
        {
            Log->Append("Client:").Append(clientId).Append(" doesn't allowed").Error();
            return false;
        }
    }
    Log->Append("Client:").Append(clientId).Append(" not found").Internal();
    return false;
}

bool SigmaWsServer::isConnectionLimitReached(String clientId, SigmaWsServer *server, AsyncWebSocketClient *client)
{
    if (server->config.maxConnectionsPerClient == 0)
    {
        return false;
    }
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
                server->clients.erase(it->second.clientNumber);
                return true;
            }
        }
    }
    return false;
}

bool SigmaWsServer::isClientLimitReached(SigmaWsServer *server, AsyncWebSocketClient *client)
{
    if (server->config.maxClients == 0)
    {
        return false;
    }
    if (server->clients.size() > server->config.maxClients)
    {
        server->Log->Append("Max clients has been reached").Error();
        client->close();
        server->clients.erase(client->id());
        return true;
    }
    return false;
}