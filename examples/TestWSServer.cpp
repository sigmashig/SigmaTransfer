#include <Arduino.h>
#include <SigmaWsServer.h>
#include <SigmaNetworkMgr.h>

SigmaLoger *Log;

SigmaConnection *protocol = NULL;

void TestPingPong()
{
}

/*
void TestSuite1()
{
  String payload = "Please, respond {'topic':'topic1','payload':'OK'}";
  SigmaInternalPkg pkg("topic1", payload.c_str());
  Log->Append("Sending message:").Append(pkg.GetPkgString()).Internal();
  esp_event_post_to(protocol->GetEventLoop(), protocol->GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, (void *)pkg.GetPkgString().c_str(), pkg.GetPkgString().length() + 1, portMAX_DELAY);
}
*/

void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  SigmaConnection *protocol = (SigmaConnection *)arg;
  if (protocol == NULL)
  {
    Log->Append("Protocol is NULL").Internal();
    return;
  }
  Log->Append("Protocol:").Append(protocol->GetName()).Append(" Base:").Append(event_base).Append("#ID: ").Append(event_id).Internal();
  if (event_id == PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE)
  {
    String pkg = String((char *)event_data);
    Log->Append("PROTOCOL_RAW_TEXT_MESSAGE:[]").Append(pkg).Internal();
  }
  else if (event_id == PROTOCOL_RECEIVED_RAW_BINARY_MESSAGE)
  {
    Log->Append("PROTOCOL_RAW_BINARY_MESSAGE:[]").Internal();
  }
  else if (event_id == PROTOCOL_RECEIVED_SIGMA_MESSAGE)
  {
    Log->Append("PROTOCOL_RECEIVED_SIGMA_MESSAGE").Internal();
    Log->Append("Event data:#").Append((char *)event_data).Append("#").Internal();
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
    Log->Append("PROTOCOL_RECEIVED_SIGMA_MESSAGE:[").Append(pkg.GetTopic()).Append("]#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else if (event_id == PROTOCOL_DISCONNECTED)
  {
    Log->Append("PROTOCOL_DISCONNECTED").Internal();
  }
  else if (event_id == PROTOCOL_CONNECTED)
  {
    char *msg = (char *)event_data;
    Log->Append("PROTOCOL_CONNECTED:").Append(msg).Append("#").Internal();
  }
  else if (event_id == PROTOCOL_AP_CONNECTED)
  {
    Log->Append("PROTOCOL_AP_CONNECTED").Internal();
  }
  else if (event_id == PROTOCOL_STA_CONNECTED)
  {
    Log->Append("PROTOCOL_STA_CONNECTED").Internal();
  }
  else if (event_id == PROTOCOL_AP_DISCONNECTED)
  {
    Log->Append("PROTOCOL_AP_DISCONNECTED").Internal();
  }
  else if (event_id == PROTOCOL_STA_DISCONNECTED)
  {
    Log->Append("PROTOCOL_STA_DISCONNECTED").Internal();
  }
  else if (event_id == PROTOCOL_ERROR)
  {
    Log->Append("PROTOCOL_ERROR").Internal();
  }
  else if (event_id == PROTOCOL_RECEIVED_PING)
  {
    Log->Append("PROTOCOL_RECEIVED_PING").Internal();
  }
  else if (event_id == PROTOCOL_RECEIVED_PONG)
  {
    Log->Append("PROTOCOL_RECEIVED_PONG").Internal();
  }
  else if (event_id == PROTOCOL_SEND_SIGMA_MESSAGE)
  {
    Log->Append("PROTOCOL_SEND_SIGMA_MESSAGE").Internal();
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
    Log->Append("PROTOCOL_SEND_SIGMA_MESSAGE:[").Append(pkg.GetTopic()).Append("]#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else
  {
    Log->Append("Unknown event:").Append(event_id).Internal();
  }
  Log->Append("Protocol event end").Internal();
}

void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  Log->Append("Network event").Internal();
  SigmaNetworkMgr *network = (SigmaNetworkMgr *)arg;
  if (network == NULL)
  {
    Log->Append("Network is NULL").Internal();
    return;
  }
  Log->Append("Base: ").Append(event_base).Append(" ID: ").Append(event_id).Internal();
  if (event_id = PROTOCOL_AP_CONNECTED)
  {
    Log->Append("PROTOCOL_AP_CONNECTED").Internal();
  }
  else if (event_id = PROTOCOL_STA_CONNECTED)
  {
    Log->Append("PROTOCOL_STA_CONNECTED").Internal();
  }
  else if (event_id = PROTOCOL_AP_DISCONNECTED)
  {
    Log->Append("PROTOCOL_AP_DISCONNECTED").Internal();
  }
  else if (event_id = PROTOCOL_STA_DISCONNECTED)
  {
    Log->Append("PROTOCOL_STA_DISCONNECTED").Internal();
  }
  else if (event_id = PROTOCOL_ERROR)
  {
    Log->Append("PROTOCOL_ERROR").Internal();
  }
  else if (event_id = PROTOCOL_RECEIVED_PING)
  {
    Log->Append("PROTOCOL_RECEIVED_PING").Internal();
  }
  else if (event_id = PROTOCOL_RECEIVED_PONG)
  {
    Log->Append("PROTOCOL_RECEIVED_PONG").Internal();
  }
  else
  {
    Log->Internal("Unknown event");
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("----Hello, World!-----");
  Log = new SigmaLoger(512);

  esp_err_t espErr = ESP_OK;
  NetworkConfig networkConfig;
  networkConfig.wifiConfig.wifiSta.ssid = "Sigma_Guest";
  networkConfig.wifiConfig.wifiSta.password = "Passwd#123";
  networkConfig.wifiConfig.wifiMode = WIFI_MODE_STA;
  networkConfig.wifiConfig.enabled = true;
  networkConfig.ethernetConfig.enabled = false;
  SigmaNetworkMgr *network = new SigmaNetworkMgr(networkConfig, Log);
  espErr = esp_event_handler_instance_register_with(
      SigmaNetworkMgr::GetEventLoop(),
      ESP_EVENT_ANY_BASE,
      ESP_EVENT_ANY_ID,
      networkEventHandler,
      network,
      NULL);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register network event handler: %d", espErr).Internal();
    exit(1);
  }
  Log->Append("Creating WS Server").Internal();

  WSServerConfig wsServerConfig;
  wsServerConfig.port = 8080;
  wsServerConfig.rootPath = "/";
  wsServerConfig.enabled = true;
  wsServerConfig.authType = AUTH_TYPE_FIRST_MESSAGE;
  wsServerConfig.pingInterval = 10;
  wsServerConfig.pingRetryCount = 3;
  SigmaWsServer *wsServer = new SigmaWsServer(wsServerConfig, Log);
  wsServer->AddAllowableClient("RM_C_Green01", "secret-api-key-12345");

  protocol = wsServer;
  Log->Append("WS Server created").Internal();
  espErr = esp_event_handler_instance_register_with(
      wsServer->GetEventLoop(),
      wsServer->GetEventBase(), // ESP_EVENT_ANY_BASE,
      ESP_EVENT_ANY_ID,
      protocolEventHandler,
      wsServer,
      NULL);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register event handler: %d", espErr).Internal();
    exit(1);
  }
  Log->Append("event handler registered").Internal();
  Log->Append("Connecting to network").Internal();
  network->Connect();
  delay(1000);
  Log->Append("Waiting for protocol to be ready").Internal();
  while (protocol->IsReady() == false)
  {
    delay(100);
  }
  Log->Append("Protocol is ready").Internal();
  TestPingPong();
  Log->Append("Setup end").Internal();
}

void loop()
{
  vTaskDelete(NULL);
}
