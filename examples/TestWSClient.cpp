#include <Arduino.h>
#include <SigmaWsClient.h>
#include <SigmaAsyncNetwork.h>

SigmaLoger *Log;

SigmaConnection *protocol = NULL;

void TestPingPong()
{
}

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
  SigmaAsyncNetwork *network = (SigmaAsyncNetwork *)arg;
  if (network == NULL)
  {
    Log->Append("Network is NULL").Internal();
    return;
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
  networkConfig.wifiConfig.enabled = false;
  networkConfig.ethernetConfig.enabled = true;
  networkConfig.ethernetConfig.hardwareType = ETHERNET_HARDWARE_TYPE_W5500;
  networkConfig.ethernetConfig.hardware.w5500.csPin = GPIO_NUM_5;
  networkConfig.ethernetConfig.hardware.w5500.rstPin = GPIO_NUM_4;
  networkConfig.ethernetConfig.dhcp = false;
  networkConfig.ethernetConfig.ip = IPAddress(192, 168, 0, 222);
  networkConfig.ethernetConfig.dns = IPAddress(8, 8, 8, 8);
  networkConfig.ethernetConfig.gateway = IPAddress(192, 168, 0, 1);
  networkConfig.ethernetConfig.subnet = IPAddress(255, 255, 255, 0);
  SigmaEthernet::GenerateMac(15, networkConfig.ethernetConfig.mac);
  SigmaAsyncNetwork *network = new SigmaAsyncNetwork(networkConfig, Log);
 /* espErr = esp_event_handler_instance_register_with(
      SigmaAsyncNetwork::GetEventLoop(),
      ESP_EVENT_ANY_BASE,
      ESP_EVENT_ANY_ID,
      networkEventHandler,
      network,
      NULL);*/
  //espErr = SigmaAsyncNetwork::RegisterEventHandlers(ESP_EVENT_ANY_ID, networkEventHandler, network);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register network event handler: %d", espErr).Internal();
  }
  Log->Append("Creating WS Client").Internal();

  WSClientConfig wsClientConfig;

  wsClientConfig.host = "ws://192.168.0.47";
  wsClientConfig.port = 8080;
  wsClientConfig.enabled = true;
  wsClientConfig.authType = AUTH_TYPE_FIRST_MESSAGE;
  wsClientConfig.apiKey = "secret-api-key-12345";
  wsClientConfig.pingType = PING_TEXT;
  wsClientConfig.pingInterval = 10;
  wsClientConfig.pingRetryCount = 3;
  SigmaWsClient *wsClient = new SigmaWsClient(wsClientConfig, Log);

  protocol = wsClient;
  Log->Append("WS Client created").Internal();
  espErr = wsClient->RegisterEventHandlers(ESP_EVENT_ANY_ID, protocolEventHandler, wsClient);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register event handler: %d", espErr).Internal();
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
