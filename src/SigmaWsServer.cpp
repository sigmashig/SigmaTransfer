#include "SigmaWsServer.h"
#include "SigmaAsyncNetwork.h"
#include <freertos/queue.h>
#include <sys/socket.h>
#include <vector>

SigmaWsServer::SigmaWsServer(WSServerConfig config, SigmaLoger *logger, int priority) : SigmaConnection("SigmaWsServer", logger, priority)
{
    this->config = config;
    isReady = false;
    pingInterval = config.pingInterval;
    retryConnectingDelay = 0; // No reconnect required

    esp_event_handler_register_with(SigmaAsyncNetwork::GetEventLoop(), SigmaAsyncNetwork::GetEventBase(), ESP_EVENT_ANY_ID, networkEventHandler, this);
    // esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_RAW_BINARY_MESSAGE, protocolEventHandler, this);
    // esp_event_handler_register_with(SigmaProtocol::GetEventLoop(), this->name.c_str(), PROTOCOL_SEND_RAW_TEXT_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, protocolEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_PING, protocolEventHandler, this);
    esp_event_handler_register_with(GetEventLoop(), GetEventBase(), PROTOCOL_SEND_PONG, protocolEventHandler, this);
    xQueue = xQueueCreate(10, sizeof(httpd_req_t *));
    requestSemaphore = xSemaphoreCreateBinaryStatic(&requestBuffer);
    xSemaphoreGive(requestSemaphore);
    xTaskCreate(processData, "ProcessData", 4096, this, 1, NULL);

    setPingTimer(this);

    serverConfig.server_port = config.port;
    serverConfig.max_open_sockets = config.maxClients;

    allowableClients.clear();
    for (auto it = config.allowableClients.begin(); it != config.allowableClients.end(); it++)
    {

        Log->Append("AllowableClient:").Append(it->second.clientId).Append("#first:").Append(it->first).Append("#").Append(it->second.authKey).Internal();
        AddAllowableClient(it->second.clientId, it->second.authKey);
    }
    clients.clear();
    shouldConnect = true;
}

SigmaWsServer::~SigmaWsServer()
{
}

esp_err_t SigmaWsServer::onWsEvent(httpd_req_t *req)
{
    if (xSemaphoreTake(requestSemaphore, portMAX_DELAY) == pdPASS)
    {
        xQueueSend(xQueue, &req, portMAX_DELAY);
        if (xSemaphoreTake(requestSemaphore, portMAX_DELAY) == pdPASS)
        {
            xSemaphoreGive(requestSemaphore);
        }
        else
        {
            Serial.printf("onWsEvent:semaphore 2 take failed\n");
        }
    }
    else
    {
        Serial.printf("onWsEvent:semaphore take failed\n");
    }
    return ESP_OK;
}

void SigmaWsServer::sendWSFrame(void *arg)
{
    TransferPkg *pkg = (TransferPkg *)arg;

    httpd_ws_frame_t wsFrame = {
        .final = true,
        .fragmented = false};

    switch (pkg->type)
    {
    case HTTPD_WS_TYPE_TEXT:
        wsFrame.type = HTTPD_WS_TYPE_TEXT;
        wsFrame.payload = (uint8_t *)pkg->message.c_str();
        wsFrame.len = pkg->message.length();
        break;
    case HTTPD_WS_TYPE_BINARY:
        wsFrame.type = HTTPD_WS_TYPE_BINARY;
        wsFrame.payload = (uint8_t *)pkg->buffer;
        wsFrame.len = pkg->len;
        break;
    case HTTPD_WS_TYPE_PING:
        wsFrame.type = HTTPD_WS_TYPE_PING;
        wsFrame.payload = (uint8_t *)pkg->message.c_str();
        wsFrame.len = pkg->message.length();
        break;
    case HTTPD_WS_TYPE_PONG:
        wsFrame.type = HTTPD_WS_TYPE_PONG;
        wsFrame.payload = (uint8_t *)pkg->message.c_str();
        wsFrame.len = pkg->message.length();
        break;
    case HTTPD_WS_TYPE_CLOSE:
        wsFrame.type = HTTPD_WS_TYPE_CLOSE;
        wsFrame.payload = NULL;
        wsFrame.len = 0;
        break;
    default:
        // Log->Printf("sendWSFrame: unknown type=%d\n", pkg->type).Error();
        break;
    }

    httpd_ws_send_frame_async(pkg->server, pkg->socketNumber, &wsFrame);
    delete pkg;
    return;
}

FullAddress SigmaWsServer::getClientFullAddress(int32_t socketNumber)
{
    struct sockaddr_in6 addr; // IPv6-compatible
    socklen_t addr_len = sizeof(addr);
    // char ip_str[INET6_ADDRSTRLEN];
    FullAddress fullAddress;

    if (getpeername(socketNumber, (struct sockaddr *)&addr, &addr_len) == 0)
    {
        // IPv4 у форматі IPv6-mapped (::FFFF:x.x.x.x)
        fullAddress.ip = IPAddress(addr.sin6_addr.s6_addr[12], addr.sin6_addr.s6_addr[13], addr.sin6_addr.s6_addr[14], addr.sin6_addr.s6_addr[15]);
        fullAddress.port = addr.sin6_port;
    }
    return fullAddress;
}

FullAddress SigmaWsServer::getClientFullAddress(httpd_req_t *req)
{
    return getClientFullAddress(httpd_req_to_sockfd(req));
}

bool SigmaWsServer::removeClient(int32_t socketNumber)
{
    // Send disconnect (close frame) to client by socket fd
    if (clients.find(socketNumber) != clients.end())
    {
        Log->Printf("removeClient: socketNumber=%d found\n", socketNumber).Warn();
        TransferPkg *pkg = new TransferPkg();
        pkg->server = server;
        pkg->socketNumber = socketNumber;
        pkg->type = HTTPD_WS_TYPE_CLOSE;
        pkg->message = "";
        pkg->buffer = NULL;
        pkg->len = 0;
        esp_err_t ret = httpd_queue_work(server, sendWSFrame, (void *)pkg);
        if (ret == ESP_OK)
        {
            ret = httpd_sess_trigger_close(server, socketNumber);
            if (ret != ESP_OK)
            {
                Log->Printf("Failed to trigger close for sockfd %d: %d\n", socketNumber, ret).Error();
            }
            clients.erase(socketNumber);
            return true;
        }
        else
        {
            Log->Printf("Failed to send disconnect to sockfd %d: %d\n", socketNumber, ret).Error();
            return false;
        }
    }
    return true;
}

int32_t SigmaWsServer::isClientConnected(String clientId)
{
    for (auto it = clients.begin(); it != clients.end(); it++)
    {
        if (it->second.clientId == clientId)
        {
            return it->first;
        }
    }
    return -1;
}

void SigmaWsServer::processData(void *arg)
{
    SigmaWsServer *serv = (SigmaWsServer *)arg;

    while (true)
    {
        httpd_req_t *req;

        if (xQueueReceive(serv->xQueue, &req, portMAX_DELAY) == pdPASS)
        {
            int32_t socketNumber = httpd_req_to_sockfd(req);
            if (req->method == HTTP_GET)
            {
                ClientAuth auth;

                auth.clientId = "";
                auth.socketNumber = socketNumber;
                auth.fullAddress = serv->getClientFullAddress(auth.socketNumber);
                auth.isAuth = false;
                // auth.pingType = PingType::PING_ONLY_BINARY;
                auth.pingRetryCount = serv->config.pingRetryCount;
                serv->clients[auth.socketNumber] = auth;

                serv->Log->Printf("WebSocket client #%u connected from %s:%d\n", auth.socketNumber, auth.fullAddress.ip.toString().c_str(), auth.fullAddress.port).Info();
                if (serv->clients.size() > serv->config.maxClients)
                {
                    serv->Log->Append("Client limit reached").Error();
                    serv->removeClient(auth.socketNumber);
                }
                else
                {
                    if (serv->config.authType == AUTH_TYPE_URL)
                    {
                        String uri = req->uri;
                        int i = uri.indexOf("?");
                        if (i != -1)
                        {
                            String params = uri.substring(i + 1);
                            String clientId = params.substring(params.indexOf("clientId=") + String("clientId=").length(), params.indexOf("&"));
                            String authKey = params.substring(params.indexOf("apiKey=") + String("apiKey=").length());
                            if (clientId.length() > 0 && authKey.length() > 0)
                            {
                                int32_t socketNumber = serv->isClientConnected(clientId);
                                if (socketNumber > 0)
                                { // client already connected. Kill it and create new one
                                    serv->Log->Printf("Client %s already connected. Killing it and creating new one\n", clientId.c_str()).Error();
                                    serv->removeClient(socketNumber);
                                }
                                if (serv->isClientAvailable(clientId, authKey))
                                {
                                    serv->Log->Printf("Client %s connected with auth key %s\n", clientId.c_str(), authKey.c_str()).Info();
                                    serv->clients[auth.socketNumber].clientId = clientId;
                                    serv->clients[auth.socketNumber].isAuth = true;
                                }
                                else
                                {
                                    serv->Log->Printf("Client %s not available or auth key does not match\n", clientId.c_str()).Error();
                                    serv->removeClient(auth.socketNumber);
                                }
                            }
                            else
                            {
                                serv->Log->Printf("Some elements of auth record didn't provided in URL \n").Error();
                                serv->removeClient(auth.socketNumber);
                            }
                        }
                        else
                        {
                            serv->Log->Printf("The auth record didn't provided in URL \n").Error();
                            serv->removeClient(auth.socketNumber);
                        }
                    }
                }
            }
            else
            {
                // data package received
                if (serv->clients.find(socketNumber) != serv->clients.end())
                {
                    httpd_ws_frame_t wsPkg;
                    memset(&wsPkg, 0, sizeof(httpd_ws_frame_t));
                    // wsPkg.type = HTTPD_WS_TYPE_TEXT;

                    // First receive the packet length
                    esp_err_t ret = httpd_ws_recv_frame(req, &wsPkg, 0);
                    if (ret != ESP_OK)
                    {
                        serv->Log->Printf("Package from client can't be processed. Error:%d", ret).Error();
                    }
                    else
                    {

                        if (wsPkg.type == HTTPD_WS_TYPE_TEXT)
                        {
                            if (wsPkg.len)
                            {
                                // Allocate space for the data
                                uint8_t *buf = (uint8_t *)calloc(1, wsPkg.len + 1);
                                if (buf == NULL)
                                {
                                    serv->Log->Printf("Failed to allocate memory for buf").Error();
                                }
                                else
                                {
                                    wsPkg.payload = buf;
                                    // Receive the packet
                                    ret = httpd_ws_recv_frame(req, &wsPkg, wsPkg.len);
                                    if (ret != ESP_OK)
                                    {
                                        serv->Log->Printf("P2.httpd_ws_recv_frame failed with %d", ret).Error();
                                    }
                                    else
                                    { // data package received
                                        // serv->Log->Printf("Data package received from client %s\n", serv->clients[socketNumber].clientId.c_str()).Internal();
                                        serv->handleTextPackage(wsPkg.payload, wsPkg.len, socketNumber, req);
                                    }
                                }
                                free(buf);
                            }
                            else
                            {
                                serv->Log->Printf("Empty package\n").Warn();
                            }
                        }
                        else if (wsPkg.type == HTTPD_WS_TYPE_BINARY)
                        {
                            serv->Log->Printf("Binary package received from client %s\n", serv->clients[socketNumber].clientId.c_str()).Internal();
                            // TODO: processing of binary package
                        }
                        else if (wsPkg.type == HTTPD_WS_TYPE_CLOSE)
                        {
                            serv->Log->Printf("Close package received from client %s\n", serv->clients[socketNumber].clientId.c_str()).Internal();
                            serv->removeClient(socketNumber);
                        }
                        else if (wsPkg.type == HTTPD_WS_TYPE_PING)
                        {
                            serv->Log->Printf("Ping package received from client %s\n", serv->clients[socketNumber].clientId.c_str()).Internal();
                            serv->sendPongToClient(serv->clients[socketNumber], String((char *)wsPkg.payload, wsPkg.len));
                        }
                        else if (wsPkg.type == HTTPD_WS_TYPE_PONG)
                        {
                            serv->Log->Printf("Pong package received from client %s\n", serv->clients[socketNumber].clientId.c_str()).Internal();
                            // TODO: processing of pong package
                        }
                        else if (wsPkg.type == HTTPD_WS_TYPE_CONTINUE)
                        {
                            serv->Log->Printf("Continue package received from client %s\n", serv->clients[socketNumber].clientId.c_str()).Internal();
                            // TODO: processing of continue package
                        }
                        else
                        {
                            serv->Log->Printf("Unknown package type:%d\n", wsPkg.type).Error();
                        }
                    }
                }
                else
                {
                    serv->Log->Printf("Socket %d not found\n", socketNumber).Error();
                    serv->removeClient(socketNumber);
                }
            }
            // free(req);
            xSemaphoreGive(requestSemaphore);
        }
    }
}

void SigmaWsServer::handleTextPackage(uint8_t *payload, size_t len, int32_t socketNumber, httpd_req_t *req)
{
    String payloadStr = String((char *)payload, len);
    Log->Printf("handleTextPackage:payloadStr=%s\n", payloadStr.c_str()).Internal();
    if (clients.find(socketNumber) == clients.end())
    {
        Log->Printf("Client %d not found\n", socketNumber).Error();
        return;
    }
    else if (clients[socketNumber].isAuth || config.authType == AUTH_TYPE_NONE)
    { // client is authenticated or auth type is none
      // Log->Append("Client is authenticated or none auth type").Internal();
        if (config.authType == AUTH_TYPE_ALL_MESSAGES)
        {
            String payload = payloadStr;
            if (clientAuthRequest(req, payload, &clients[socketNumber], payloadStr))
            {
                SendMessage(payload, PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE);
            }
            else
            {
                Log->Printf("Client %d is not authenticated\n", socketNumber).Error();
                removeClient(socketNumber);
            }
        }
        String uPayload = payloadStr;
        uPayload.toUpperCase();
        if (uPayload.startsWith("PING"))
        {
            Log->Printf("PING command received from client %s\n", clients[socketNumber].clientId.c_str()).Internal();
            sendPongToClient(clients[socketNumber], String((char *)payload + strlen("PING"), len - strlen("PING")));
        }
        else if (uPayload.startsWith("PONG"))
        {
            Log->Printf("PONG command received from client %s\n", clients[socketNumber].clientId.c_str()).Internal();
            // TODO: processing of pong package
        }
        else if (uPayload.startsWith("CLOSE"))
        {
            Log->Printf("CLOSE command received from client %s\n", clients[socketNumber].clientId.c_str()).Internal();
            removeClient(socketNumber);
        }
        else if (SigmaInternalPkg::IsSigmaInternalPkg(payloadStr))
        {
            SigmaInternalPkg pkg(payloadStr);
            if (pkg.IsError())
            {
                Log->Printf("Error in package: %s\n", pkg.GetPkgString().c_str()).Error();
            }
            else
            {
                auto subscription = subscriptions.find(pkg.GetTopic());
                if (subscription != subscriptions.end())
                {
                    // server->Log->Append("Sending event to subscription:").Append(server->name).Append("#").Append(subscription->second.eventId).Internal();
                    SendMessage(pkg.GetPayload(), subscription->second.eventId);
                }
                SendMessage(pkg.GetPkgString(), PROTOCOL_RECEIVED_SIGMA_MESSAGE);
            }
        }
        else
        {
            Log->Printf("Unknown package: %s\n", payloadStr.c_str()).Error();
        }
    }
    else if (config.authType == AUTH_TYPE_FIRST_MESSAGE)
    {
        String dummy;
        if (clientAuthRequest(req, payloadStr, &clients[socketNumber], dummy))
        {
            // SendMessage(payloadStr, PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE);
            Log->Printf("SUCCESS! Client[%d] %s is authenticated\n", socketNumber, clients[socketNumber].clientId.c_str()).Info();
        }
        else
        {
            Log->Printf("Client %d is not authenticated\n", socketNumber).Error();
            removeClient(socketNumber);
        }
    }
    else
    { // client is not authenticated and auth type is not none
        Log->Printf("Client %d is not authenticated\n", socketNumber).Error();
        removeClient(socketNumber);
    }
}

bool SigmaWsServer::clientAuthRequest(httpd_req_t *req, String &request, ClientAuth *auth, String &payload)
{
    // Log->Append("clientAuthRequest:request=").Append(request).Internal();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, request);
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
        if (req != NULL)
        {
            std::vector<int32_t> toRemove;
            for (auto it = clients.begin(); it != clients.end(); it++)
            {
                if (it->second.clientId == cId && it->first != auth->socketNumber)
                { // duplicated clientId
                    Log->Printf("clientAuthRequest: duplicated clientId: %s:%d\n", cId.c_str(), it->first).Error();
                    toRemove.push_back(it->first);
                }
            }
            for (auto it = toRemove.begin(); it != toRemove.end(); it++)
            {
                removeClient(*it);
            }
        }
        auth->clientId = cId;
        auth->isAuth = true;
        if (payload != nullptr)
        {
            payload = doc["payload"].as<String>();
        }
        return true;
    }
    return false;
}

void SigmaWsServer::Connect()
{
    if (!shouldConnect)
    {
        return;
    }

    // Start server
    Log->Append("Connecting to WebSocket server:").Internal();
    /*
    Log->Append("serverConfig.server_port:").Append(serverConfig.server_port).Internal();
    Log->Append("serverConfig.max_open_sockets:").Append(serverConfig.max_open_sockets).Internal();
    Log->Append("serverConfig.max_uri_handlers:").Append(serverConfig.max_uri_handlers).Internal();
    Log->Append("serverConfig.max_resp_headers:").Append(serverConfig.max_resp_headers).Internal();
    Log->Append("serverConfig.backlog_conn:").Append(serverConfig.backlog_conn).Internal();
    Log->Append("serverConfig.lru_purge_enable:").Append(serverConfig.lru_purge_enable).Internal();
    Log->Append("serverConfig.recv_wait_timeout:").Append(serverConfig.recv_wait_timeout).Internal();
    Log->Append("serverConfig.stack_size:").Append(serverConfig.stack_size).Internal();
    Log->Append("serverConfig.task_priority:").Append(serverConfig.task_priority).Internal();
    Log->Append("serverConfig.core_id:").Append(serverConfig.core_id).Internal();
    Log->Append("serverConfig.ctrl_port:").Append(serverConfig.ctrl_port).Internal();
    Log->Append("Free heap: ").Append(esp_get_free_heap_size()).Internal();
    */
    if (httpd_start(&server, &serverConfig) == ESP_OK)
    {
        // Register WebSocket handler
        httpd_uri_t ws_uri = {
            .uri = config.rootPath.c_str(),
            .method = HTTP_GET,
            .handler = onWsEvent,
            .user_ctx = NULL,
            .is_websocket = true};
        httpd_register_uri_handler(server, &ws_uri);

        Log->Append("WebSocket server started on ws://").Append(WiFi.localIP().toString()).Append(config.rootPath).Append(":").Append(config.port).Internal();
        setReady(true);
        SendMessage("CONNECTED", PROTOCOL_CONNECTED);
    }
    else
    {
        Log->Append("Failed to start WebSocket server").Error();
    }
}

void SigmaWsServer::Disconnect()
{
    httpd_stop(server);
    httpd_unregister_uri_handler(server, config.rootPath.c_str(), HTTP_GET);
    setReady(false);
    SendMessage("DISCONNECTED", PROTOCOL_DISCONNECTED);
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

bool SigmaWsServer::isClientAvailable(String clientId, String authKey)
{
    if (allowableClients.find(clientId) != allowableClients.end())
    {
        if (allowableClients[clientId].authKey == authKey)
        {
            return true;
        }
    }
    return false;
}

void SigmaWsServer::sendPing()
{
    Log->Printf("PingTask(%d)", clients.size()).Internal();
    std::vector<int32_t> clientsToRemove;
    clientsToRemove.clear();
    for (auto it = clients.begin(); it != clients.end(); it++)
    {
        // Log->Append("PingTask: client:").Append(it->second.clientId).Append("#").Append("#").Append(it->second.pingRetryCount).Internal();
        bool res = sendPingToClient(it->second, "PING", false);
        if (!res)
        {
            Log->Append("Failed to send ping to client:").Append(it->second.clientId).Append("#").Append(it->second.pingRetryCount).Error();
        }
        else
        {
            Log->Append("Ping sent to client:").Append(it->second.clientId).Internal();
        }
        it->second.pingRetryCount--;
        if (it->second.pingRetryCount < 0)
        {
            Log->Append("Client " + it->second.clientId + " disconnected due to ping timeout").Error();
            SendMessage("PING_TIMEOUT.  Client:" + it->second.clientId + " disconnected.", PROTOCOL_PING_TIMEOUT);
            clientsToRemove.push_back(it->first);
        }
    }
    for (auto it = clientsToRemove.begin(); it != clientsToRemove.end(); it++)
    {
        removeClient(*it);
    }
}

bool SigmaWsServer::sendPingToClient(ClientAuth &auth, String payload, bool isRemoveClient)
{
    bool res = false;
    PingType pingType = auth.pingType;
    if (pingType == PING_NO)
    {
        res = true;
    }
    if (pingType == PING_TEXT)
    {
        sendMessageToClient(auth, "PING");
        res = true;
    }
    else if (pingType == PING_BINARY)
    {
        res = sendPingToClient(auth.socketNumber, "PING");
    }
    if (!res)
    {
        Log->Append("Failed to send ping to client:").Append(auth.clientId).Append("#").Append(auth.pingRetryCount).Error();
    }
    if (isRemoveClient)
    {
        auth.pingRetryCount--;
        if (auth.pingRetryCount < 0)
        {
            Log->Append("[2]Client " + auth.clientId + " disconnected due to ping timeout").Error();
            removeClient(auth.socketNumber);
            SendMessage("PING_TIMEOUT.  Client:" + auth.clientId + " disconnected.", PROTOCOL_PING_TIMEOUT);
        }
    }
    return res;
}

bool SigmaWsServer::sendPongToClient(ClientAuth &auth, String payload)
{
    bool res = false;
    PingType pingType = auth.pingType;
    if (pingType == PING_NO)
    {
        res = true;
    }
    if (pingType == PING_TEXT)
    {
        sendMessageToClient(auth, "PONG");
        res = true;
    }
    else if (pingType == PING_BINARY)
    {
        res = sendPongToClient(auth, "PONG");
    }
    if (!res)
    {
        Log->Append("Failed to send pong to client:").Append(auth.clientId).Append("#").Append(auth.pingRetryCount).Error();
    }
    auth.pingRetryCount--;
    if (auth.pingRetryCount < 0)
    {
        Log->Append("[3]Client " + auth.clientId + " disconnected due to ping timeout").Error();
        removeClient(auth.socketNumber);
        SendMessage("PING_TIMEOUT.  Client:" + auth.clientId + " disconnected.", PROTOCOL_PING_TIMEOUT);
    }
    return res;
}

bool SigmaWsServer::sendMessageToClient(ClientAuth &auth, String message)
{
    return sendMessageToClient(auth.socketNumber, message);
}

bool SigmaWsServer::sendMessageToClient(int32_t socketNumber, String message)
{
    TransferPkg *pkg = new TransferPkg();
    pkg->server = server;
    pkg->socketNumber = socketNumber;
    pkg->type = HTTPD_WS_TYPE_TEXT;
    pkg->message = message;
    pkg->buffer = NULL;
    pkg->len = 0;
    return (ESP_OK == httpd_queue_work(server, sendWSFrame, (void *)pkg));
}

bool SigmaWsServer::sendPongToClient(int32_t socketNumber, String message)
{
    TransferPkg *pkg = new TransferPkg();
    pkg->server = server;
    pkg->socketNumber = socketNumber;
    pkg->type = HTTPD_WS_TYPE_PONG;
    pkg->message = message;
    pkg->buffer = NULL;
    pkg->len = 0;
    return (ESP_OK == httpd_queue_work(server, sendWSFrame, (void *)&pkg));
}

bool SigmaWsServer::sendPingToClient(int32_t socketNumber, String message)
{

    TransferPkg *pkg = new TransferPkg();
    pkg->server = server;
    pkg->socketNumber = socketNumber;
    pkg->type = HTTPD_WS_TYPE_PING;
    pkg->message = message;
    pkg->buffer = NULL;
    pkg->len = 0;
    return (ESP_OK == httpd_queue_work(server, sendWSFrame, (void *)pkg));
}

void SigmaWsServer::protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    SigmaWsServer *ws = (SigmaWsServer *)arg;
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
    ClientAuth *auth = nullptr;

    for (auto it = ws->clients.begin(); it != ws->clients.end(); it++)
    {
        if (it->second.clientId == pkg.GetClientId())
        {
            auth = &it->second;
            break;
        }
    }
    if (auth == nullptr)
    {
        ws->Log->Append("Client not found").Error();
        return;
    }
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
            // Serial.println(pkg.GetPkgString().c_str());
            // res = ws->sendMessageToClient(pkg.GetClientId(), pkg.GetBinaryPayload(), pkg.GetBinaryPayloadLength());
        }
        else
        {
            res = ws->sendMessageToClient(*auth, pkg.GetPkgString());
        }
        if (!res)
        {
            ws->Log->Append("Failed to send message to client: ").Append(pkg.GetClientId()).Error();
        }
    }
    else if (event_id == PROTOCOL_SEND_PING)
    {
        // Nothing to do here
        // ws->sendPingToClient(auth, pkg.GetPayload());
    }
    else if (event_id == PROTOCOL_SEND_PONG)
    {
        // Nothing to do here
        // ws->sendPongToClient(auth, pkg.GetPayload());
    }
}
