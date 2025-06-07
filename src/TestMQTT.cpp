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
// SigmaLoger *ZelorLog = new SigmaLoger(0);

enum
{
  EVENT_TEST1 = 0x80,
  EVENT_TEST2 = 0x81,
  EVENT_TEST3 = 0x82,
  EVENT_TEST4 = 0x83,
  EVENT_TEST5 = 0x84
} MY_EVENT_IDS;

// int step = 0;
// SigmaProtocol *master = NULL;
// SigmaProtocol *slave = NULL;
SigmaProtocol *protocol = NULL;
/*
void slaveHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  Log->Append("Slave event#").Append(step).Internal();
  Log->Append("Slave.Base: ").Append(event_base).Append(" ID: ").Append(event_id).Internal();
  SigmaProtocol *protocol = (SigmaProtocol *)arg;
  if (protocol == NULL)
  {
    Log->Append("Protocol is NULL").Internal();
    return;
  }
  if (event_id == PROTOCOL_CONNECTED)
  {
    Log->Append("Slave connected").Internal();
    return;
  }
  if (event_id == PROTOCOL_DISCONNECTED)
  {
    Log->Append("Slave disconnected").Internal();
    return;
  }
  switch (step)
  {
  // case 0:
  //{ // authentication
  //   Log->Append("[ERROR!!!]Should not be here on step 0").Error();
  //   break;
  // }
  case 1:
  {
    if (event_id == PROTOCOL_RECEIVED_SIGMA_MESSAGE)
    {
      SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
      Log->Append("PROTOCOL_MESSAGE:[").Append(pkg.GetTopic()).Append("]:#").Append(pkg.GetPayload()).Append("#").Internal();
    }
    else if (event_id == EVENT_TEST1)
    {
      SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
      Log->Append("EVENT_TEST1:[").Append(pkg.GetTopic()).Append("]:#").Append(pkg.GetPayload()).Append("#").Internal();
      step++;
      SigmaInternalPkg pkg2("ping", "Waiting for ping");
      // esp_event_post_to(protocol->GetEventLoop(), protocol->GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, (void *)&pkg2, sizeof(SigmaInternalPkg), portMAX_DELAY);
      break;
    }
  }
  }
}
*/
/*
void masterHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  Log->Append("Master event#").Append(step).Internal();
  Log->Append("Master.Base: ").Append(event_base).Append(" ID: ").Append(event_id).Internal();

  SigmaProtocol *protocol = (SigmaProtocol *)arg;
  if (protocol == NULL)
  {
    Log->Append("Protocol is NULL").Internal();
    return;
  }
  if (event_id == PROTOCOL_CONNECTED)
  {
    Log->Append("Master connected").Internal();
    return;
  }
  if (event_id == PROTOCOL_DISCONNECTED)
  {
    Log->Append("Master disconnected").Internal();
    return;
  }
  switch (step)
  {
  case 0:
  { // authentication
    if (event_id == PROTOCOL_RECEIVED_RAW_TEXT_MESSAGE)
    {
      String msg = String((char *)event_data);
      Log->Append("PROTOCOL_RAW_TEXT_MESSAGE:[").Append(msg).Append("]").Internal();
      step++;
      String msg2 = "{'clientId':'xxxW123','apiKey':'secret-api-key-12345', 'topic':'fromMaster', 'payload':'wrong message to unknown client'}";
      // esp_event_post_to(protocol->GetEventLoop(), protocol->GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, (void *)msg2.c_str(), msg2.length(), portMAX_DELAY);
    }
    break;
  }
  case 2:
  { // receive ping request
    if (event_id == PROTOCOL_RECEIVED_SIGMA_MESSAGE)
    {
      SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
      Log->Append("PROTOCOL_RECEIVED_SIGMA_MESSAGE:[").Append(pkg.GetTopic()).Append("]:#").Append(pkg.GetPayload()).Append("#").Internal();
    }
    else if (event_id == EVENT_TEST2)
    {
      SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
      Log->Append("EVENT_TEST2:[").Append(pkg.GetTopic()).Append("]:#").Append(pkg.GetPayload()).Append("#").Internal();
      step++;
      esp_event_post_to(protocol->GetEventLoop(), protocol->GetEventBase(), PROTOCOL_SEND_PING, (void *)pkg.GetPayload().c_str(), pkg.GetPayload().length(), portMAX_DELAY);
    }
    break;
  }
  case 4:
  { // receive pong request
    if (event_id == PROTOCOL_RECEIVED_PONG)
    {
      Log->Append("PROTOCOL_RECEIVED_PONG:").Append((char *)event_data).Internal();
    }
    break;
  }
  }
}
*/

void TestSuite1()
{
  Log->Append("TestSuite1").Internal();
  TopicSubscription subscription;
  // subscription for slave
  subscription.topic = "topic1";
  subscription.eventId = EVENT_TEST1;
  subscription.isReSubscribe = true;
  //Log->Printf("Protocol:%p#", protocol).Internal();
  protocol->Subscribe(subscription);
  // subscription for master
  subscription.topic = "topic2";
  subscription.eventId = EVENT_TEST2;
  subscription.isReSubscribe = true;
  protocol->Subscribe(subscription);

  Log->Append("Waiting for protocol to be ready").Internal();
  while (protocol->IsReady() == false)
  {
    delay(100);
  }
  SigmaInternalPkg pkg("topic1", "Hello from topic");
  Log->Append("Protocol is ready").Internal();
  esp_event_post_to(protocol->GetEventLoop(), protocol->GetEventBase(), PROTOCOL_SEND_SIGMA_MESSAGE, (void *)pkg.GetPkgString().c_str(), pkg.GetPkgString().length()+1, portMAX_DELAY);
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
    Log->Append("PROTOCOL_RAW_BINARY_MESSAGE:[]").Internal();
  }
  else if (event_id == PROTOCOL_RECEIVED_SIGMA_MESSAGE)
  {
    Log->Append("PROTOCOL_RECEIVED_SIGMA_MESSAGE").Internal();
    Log->Append("Event data:#").Append((char *)event_data).Append("#").Internal();
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
    Log->Append("PROTOCOL_MESSAGE:[").Append(pkg.GetTopic()).Append("]#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else if (event_id == PROTOCOL_DISCONNECTED)
  {
    Log->Append("PROTOCOL_DISCONNECTED").Internal();
  }
  else if (event_id == PROTOCOL_CONNECTED)
  {
    char *msg = (char *)event_data;
    Log->Append("PROTOCOL_CONNECTED:").Append(msg).Append("#").Internal();
    TestSuite1();
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
  else if (event_id == EVENT_TEST1)
  {
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
    Log->Append("EVENT_TEST1:[").Append(pkg.GetTopic()).Append("]:#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else if (event_id == EVENT_TEST2)
  {
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
    Log->Append("EVENT_TEST2:[").Append(pkg.GetTopic()).Append("]:#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else if (event_id == EVENT_TEST3)
  {
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
    Log->Append("EVENT_TEST3:[").Append(pkg.GetTopic()).Append("]:#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else if (event_id == EVENT_TEST4)
  {
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
    Log->Append("EVENT_TEST4:[").Append(pkg.GetTopic()).Append("]:#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else if (event_id == EVENT_TEST5)
  {
    SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
    Log->Append("EVENT_TEST5:[").Append(pkg.GetTopic()).Append("]:#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else
  {
    Log->Internal("Unknown event");
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
  Log1 = new SigmaLoger(512);

  //esp_event_loop_create_default();
  esp_err_t espErr = ESP_OK;
  /*
    esp_event_loop_args_t loop_args = {
        .queue_size = 100,
        .task_name = "SigmaTransfer_event_loop",
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = 0};
    esp_event_loop_handle_t eventLoop = NULL;
    espErr = esp_event_loop_create(&loop_args, &eventLoop);
    if (espErr != ESP_OK)
    {
      Log->Printf("Failed to create event loop: %d", espErr).Internal();
      exit(1);
    }

    esp_event_loop_handle_t eventLoop = NULL;
    espErr = esp_event_loop_create(&loop_args, &eventLoop);
    if (espErr != ESP_OK)
    {
      Log->Printf("Failed to create event loop: %d", espErr).Internal();
      exit(1);
    }
    */
  WiFiConfigSta wifiConfig;
  wifiConfig.ssid = "Sigma";
  wifiConfig.password = "kybwynyd";
  SigmaAsyncNetwork *network = new SigmaAsyncNetwork(wifiConfig, Log1);
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
  /*
  network->Connect();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    printf("Waiting for Wi-Fi...\n");
  }
*/
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
  Log->Append("Creating MQTT").Internal();
  MqttConfig mqttConfig;
  mqttConfig.server = "192.168.0.102";
  mqttConfig.rootTopic = "test/test1/";
  mqttConfig.username = "test";
  mqttConfig.password = "password";
  SigmaMQTT *Mqtt = new SigmaMQTT("MQTT", Log, mqttConfig);
  Log->Append("MQTT created").Internal();
  protocol = Mqtt;
  espErr = esp_event_handler_instance_register_with(
      Mqtt->GetEventLoop(),
      Mqtt->GetEventBase(), // ESP_EVENT_ANY_BASE,
      ESP_EVENT_ANY_ID,
      protocolEventHandler,
      Mqtt,
      NULL);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register event handler: %d", espErr).Internal();
    exit(1);
  }

#endif
  /*
    ulong eln = (ulong)network->GetEventLoop();
    ulong elm = (ulong)master->GetEventLoop();
    ulong els = (ulong)slave->GetEventLoop();
    Log->Append("Event loop:#").Append(eln).Append("#").Append(els).Append("#").Append(elm).Append("#").Internal();
  */
  //Log->Append("Connecting to network").Internal();
  network->Connect();
  //Log->Append("Connecting to network end").Internal();
  delay(1000);
  /*
  Log->Append("Waiting for protocol to be ready").Internal();
  while(protocol->IsReady() == false)
  {
    delay(100);
  }
  */
  // Log->Append("Protocol is ready").Internal();
  // TestSuite1();

  // ulong loopHandle = (ulong)SigmaProtocol::GetEventLoop();
  // Log->Append("Loop handle:").Append(loopHandle).Internal();
  // esp_event_post_to(SigmaProtocol::GetEventLoop(), "generic", 67, (void *)"Start\0", 6, portMAX_DELAY);
  Log->Append("Setup end").Internal();
}

void loop()
{
  vTaskDelete(NULL);
}
