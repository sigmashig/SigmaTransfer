#include <Arduino.h>
#include <SigmaMQTT.h>
#include <SigmaWsClient.h>
#include <SigmaAsyncNetwork.h>
#include <SigmaWsServer.h>

#define PROTO_MQTT 1
#define PROTO_UART 0
#define PROTO_WS_CLIENT 0
#define PROTO_WS_SERVER 0

SigmaLoger *Log;
SigmaLoger *Log1;
SigmaLoger *ZelorLog = new SigmaLoger(0);
SigmaProtocol *prot = NULL;
enum
{
  EVENT_TEST1 = 0x80,
  EVENT_TEST2 = 0x81,
  EVENT_TEST3 = 0x82,
  EVENT_TEST4 = 0x83,
  EVENT_TEST5 = 0x84
} MY_EVENT_IDS;

void TestSuite1()
{
  TopicSubscription subscription;
  // subscription
  subscription.topic = "topic1";
  subscription.eventId = EVENT_TEST1;
  subscription.isReSubscribe = true;
  prot->Subscribe(subscription);

  // subscription for master
  subscription.topic = "topic2";
  subscription.eventId = EVENT_TEST2;
  subscription.isReSubscribe = true;
  prot->Subscribe(subscription);

  // authentication
  Log->Append("Waiting for protocol to be ready").Internal();
  while (prot->IsReady() == false)
  {
    delay(100);
  }
  Log->Append("Protocol is ready").Internal();
  SigmaInternalPkg pkg("topic1", "Hello, World!");
  esp_event_post_to(prot->GetEventLoop(), prot->GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, (void *)&pkg, sizeof(SigmaInternalPkg), portMAX_DELAY);
  SigmaInternalPkg pkg2("topic2", "2Hello, World!");
  esp_event_post_to(prot->GetEventLoop(), prot->GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, (void *)&pkg2, sizeof(SigmaInternalPkg), portMAX_DELAY);
}
/*
void genericEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  Log->Append("Generic event").Internal();
  Log->Append("Base: ").Append(event_base).Append(" ID: ").Append(event_id).Internal();
}
*/

void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  Log->Append("Protocol event").Internal();
  SigmaProtocol *protocol = (SigmaProtocol *)arg;
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
    Log->Append("PROTOCOL_RAW_BINARY_MESSAGE:[][]").Internal();
  }
  else if (event_id = PROTOCOL_RECEIVED_SIGMA_MESSAGE)
  {
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
    Log->Append("PROTOCOL_MESSAGE:[").Append(pkg.GetTopic()).Append("]:#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else if (event_id = PROTOCOL_DISCONNECTED)
  {
    Log->Append("PROTOCOL_DISCONNECTED").Internal();
  }
  else if (event_id = PROTOCOL_CONNECTED)
  {
    Log->Append("PROTOCOL_CONNECTED").Internal();
  }
  else if (event_id = PROTOCOL_AP_CONNECTED)
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

void networkEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  Log->Append("Network event").Internal();
  SigmaAsyncNetwork *network = (SigmaAsyncNetwork *)arg;
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
  SigmaProtocol *protocol = NULL;
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("----Hello, World!-----");
  Log = new SigmaLoger(512);
  // Log1 = new SigmaLoger(512);
  esp_err_t espErr = ESP_OK;

  WiFiConfigSta wifiConfig;
  wifiConfig.ssid = "Sigma";
  wifiConfig.password = "kybwynyd";
  SigmaAsyncNetwork *network = new SigmaAsyncNetwork(wifiConfig, Log);
  espErr = esp_event_handler_instance_register_with(
      SigmaAsyncNetwork::GetEventLoop(),
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

#if PROTO_WS_SERVER == 1
  Log->Append("Creating WS server").Internal();
  WSServerConfig wsServerConfig;
  wsServerConfig.port = 8080;
  wsServerConfig.rootPath = "/";
  wsServerConfig.authType = AUTH_TYPE_FIRST_MESSAGE;
  wsServerConfig.maxClients = 10;
  wsServerConfig.maxConnectionsPerClient = 1;
  SigmaWsServer *WServer = new SigmaWsServer("WSserver", Log, wsServerConfig);
  protocol = WServer;
  WServer->AddAllowableClient("W123", "secret-api-key-12345");
  espErr = esp_event_handler_instance_register_with(
      WServer->GetEventLoop(),
      WServer->GetEventBase(), // ESP_EVENT_ANY_BASE,
      ESP_EVENT_ANY_ID,
      masterHandler,
      WServer,
      NULL);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register event handler: %d", espErr).Internal();
    exit(1);
  }
  master = WServer;
  // esp_event_post_to(SigmaProtocol::GetEventLoop(), name.c_str(), 123, (void*)"Zero \0", 6, portMAX_DELAY);
  // WServer->Connect();

#endif

#if PROTO_WS_CLIENT == 1

  Log->Append("Creating WS client").Internal();
  WSClientConfig wsClientConfig;
  wsClientConfig.host = "192.168.0.47";
  wsClientConfig.port = 8080;
  wsClientConfig.rootPath = "/";
  wsClientConfig.authType = AUTH_TYPE_FIRST_MESSAGE;
  SigmaWsClient *WClient = new SigmaWsClient("WSclient", Log, wsClientConfig);
  // protocol = WClient;
  // WClient->AddAllowableClient("W123", "secret-api-key-12345");

  espErr = esp_event_handler_instance_register_with(
      WClient->GetEventLoop(),
      WClient->GetEventBase(),
      ESP_EVENT_ANY_ID,
      slaveHandler,
      WClient,
      NULL);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register event handler: %d", espErr).Internal();
    exit(1);
  }

  Log->Append("WS client created").Internal();
  slave = WClient;
#endif

#if PROTO_UART == 1
  UartConfig uartConfig;
  uartConfig.txPin = 1;
  uartConfig.rxPin = 2;
  uartConfig.baudRate = 9600;

  SigmaProtocol *Uart = new SigmaUART(uartConfig);
  Transfer->AddProtocol("UART", Uart);

#endif
#if PROTO_MQTT == 1
  MqttConfig mqttConfig;
  mqttConfig.server = "192.168.0.102";
  mqttConfig.rootTopic = "test/test1/";
  mqttConfig.username = "test";
  mqttConfig.password = "password";

  SigmaProtocol *Mqtt = new SigmaMQTT(mqttConfig);
#endif
  /*
    ulong eln = (ulong)network->GetEventLoop();
    ulong elm = (ulong)master->GetEventLoop();
    ulong els = (ulong)slave->GetEventLoop();
    Log->Append("Event loop:#").Append(eln).Append("#").Append(els).Append("#").Append(elm).Append("#").Internal();
  */

  network->Connect();
  Log->Append("Network connected").Internal();
  delay(1000);
  TestSuite1();
  // ulong loopHandle = (ulong)SigmaProtocol::GetEventLoop();
  // Log->Append("Loop handle:").Append(loopHandle).Internal();
  // esp_event_post_to(SigmaProtocol::GetEventLoop(), "generic", 67, (void *)"Start\0", 6, portMAX_DELAY);
}

void loop()
{
  vTaskDelete(NULL);
}
