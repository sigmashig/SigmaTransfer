#include "SigmaWsServer.h"
#include "SigmaInternalPkg.h"
#include "SigmaAsyncNetwork.h"
#include "SigmaProtocol.h"
#include <esp_event.h>
#include <ArduinoJson.h>

SigmaWsServer::SigmaWsServer(String name, WSServerConfig config)
{
    this->name = name;
    this->config = config;
    isReady = false;

    xQueue = xQueueCreate(10, sizeof(SigmaWsServerData));
    xTaskCreate(processData, "ProcessData", 4096, this, 1, NULL);
    esp_event_handler_register_with(SigmaAsyncNetwork::GetEventLoop(), SIGMAASYNCNETWORK_EVENT, ESP_EVENT_ANY_ID, networkEventHandler, this);
    // esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_RAW_BINARY_MESSAGE, protocolEventHandler, this);
    // esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_RAW_TEXT_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_SIGMA_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_PING, protocolEventHandler, this);
    esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_PONG, protocolEventHandler, this);

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

    const ClientAuth* auth = GetClientAuth(clientId);
    if (auth == nullptr || auth->isAuth == false)
    {
        return false;
    }
    return auth->wsClient->text(message);
}

bool SigmaWsServer::sendMessageToClient(String clientId, byte *data, size_t size)
{
    const ClientAuth* auth = GetClientAuth(clientId);
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
    return auth->wsClient->ping((byte*)(payload.c_str()), payload.length());
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
    ClientAuth auth = server->clients[data.wsClient->id()];
    if (data.frameInfo->opcode == WS_BINARY)
    {
        if (auth.isAuth || server->config.authType == AUTH_TYPE_NONE)
        { // Binary format is available for authenticated clients (URL/FIRST MESSAGE) or no auth
            // Client MUST BE AUTHENTICATED before sending binary data
            // there is no check of auth attribute for AUTH_TYPE_ALL_MESSAGES
            SigmaInternalPkg pkg(server->name, "", data.data, data.len, true, auth.clientId);
            esp_event_post_to(GetEventLoop(), server->name.c_str(), PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE, (void *)(pkg.GetPkgString().c_str()), sizeof(pkg.GetPkgString()), portMAX_DELAY);
        }
        else
        {
            server->Log->Printf("Client %s is not authenticated\n", auth.clientId.c_str()).Error();
            data.wsClient->close();
            server->clients.erase(data.wsClient->id());
        }
    }
    else if (data.frameInfo->opcode == WS_TEXT)
    {
        String payload = String((char *)data.data);
        if ((!auth.isAuth && server->config.authType == AUTH_TYPE_FIRST_MESSAGE) || server->config.authType == AUTH_TYPE_ALL_MESSAGES)
        {
            if (server->clientAuthRequest(payload))
            {
                auth.isAuth = true;
                auth.clientId = data.wsClient->id();
            }
            else
            {
                server->Log->Printf("Client %s is not authenticated", auth.clientId.c_str()).Error();
                data.wsClient->close();
                server->clients.erase(data.wsClient->id());
            }
        }
        esp_event_post_to(GetEventLoop(), server->name.c_str(), PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE, (void *)(payload.c_str()), sizeof(payload), portMAX_DELAY);
        if (SigmaInternalPkg::IsSigmaInternalPkg(payload))
        {
            SigmaInternalPkg pkg(payload);
            esp_event_post_to(GetEventLoop(), server->name.c_str(), PROTOCOL_RECEIVED_SIGMA_MESSAGE, (void *)(pkg.GetPkgString().c_str()), sizeof(pkg.GetPkgString()), portMAX_DELAY);
            auto subscription = server->subscriptions.find(pkg.GetTopic());
            if (subscription != server->subscriptions.end())
            {
                esp_event_post_to(SigmaProtocol::GetEventLoop(), server->name.c_str(), subscription->second.eventId, (void *)(&pkg), sizeof(pkg), portMAX_DELAY);
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

bool SigmaWsServer::clientAuthRequest(String payload)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Log->Append("Failed to deserialize JSON: ").Append(error.c_str()).Error();
        return false;
    }
    String clientId = doc["clientId"].as<String>();
    String authKey = doc["authKey"].as<String>();
    if (isClientAvailable(clientId, authKey))
    {
        return true;
    }
    return false;
}
