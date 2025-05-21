#include <Arduino.h>
// #include <SigmaUART.h>
// #include <SigmaMQTT.h>
#include <SigmaWsClient.h>
#include <SigmaAsyncNetwork.h>
// #include <SigmaProtocolDefs.h>

#define PROTO_MQTT 0
#define PROTO_UART 0
#define PROTO_WS_CLIENT 1
#define PROTO_WS_SERVER 0
enum
{
  EVENT_TEST1 = 0x80,
  EVENT_TEST2 = 0x81,
  EVENT_TEST3 = 0x82,
  EVENT_TEST4 = 0x83,
  EVENT_TEST5 = 0x84
} MY_EVENT_IDS;
void TestDelayedMessaging(String protocolName, esp_event_loop_handle_t eventLoop)
{

  SigmaInternalPkg pkg(protocolName, "delayedTopic1", "Hello, World!");
  esp_event_post_to(eventLoop, protocolName.c_str(), ESP_EVENT_ANY_ID, &pkg, sizeof(SigmaInternalPkg), portMAX_DELAY);
}

void TestMessaging(SigmaProtocol *protocol, esp_event_loop_handle_t eventLoop)
{
  if (protocol != NULL)
  {
    TopicSubscription subscription;
    subscription.topic = "subscriptionTopic1";
    subscription.eventId = EVENT_TEST1;
    subscription.isReSubscribe = true;
    protocol->Subscribe(subscription);
    subscription.topic = "subscriptionTopic2";
    subscription.eventId = EVENT_TEST2;
    subscription.isReSubscribe = true;
    protocol->Subscribe(subscription);

    SigmaInternalPkg pkg(protocol->GetName(), "testTopic1", "Hello, World!");
    esp_event_post_to(eventLoop, protocol->GetName().c_str(), ESP_EVENT_ANY_ID, &pkg, sizeof(SigmaInternalPkg), portMAX_DELAY);

    SigmaInternalPkg pkg2(protocol->GetName(), "testTopic2", "Hello, World!");
    esp_event_post_to(eventLoop, protocol->GetName().c_str(), ESP_EVENT_ANY_ID, &pkg2, sizeof(SigmaInternalPkg), portMAX_DELAY);
  }
}

void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  SigmaProtocol *protocol = (SigmaProtocol *)arg;
  if (protocol == NULL)
  {
    Log->Append("Protocol is NULL").Internal();
    return;
  }
  Log->Append("Base: ").Append(event_base).Append(" ID: ").Append(event_id).Append(" Protocol: ").Append(protocol->GetName()).Internal();
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
    Log->Append("PROTOCOL_MESSAGE:[").Append(pkg.GetTopic()).Append("]:").Append(pkg.GetPayload()).Internal();
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
  SigmaAsyncNetwork *network = new SigmaAsyncNetwork(wifiConfig);
  espErr = esp_event_handler_instance_register_with(
      SigmaAsyncNetwork::GetEventLoop(),
      "network",
      ESP_EVENT_ANY_ID,
      networkEventHandler,
      network,
      NULL);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register network event handler: %d", espErr).Internal();
    exit(1);
  }

#if PROTO_WS_CLIENT == 1
  {
    WSClientConfig wsConfig;
    String name = "WS";
    wsConfig.host = "192.168.0.102";
    wsConfig.port = 8080;
    wsConfig.rootPath = "/";
    wsConfig.authType = AUTH_TYPE_FIRST_MESSAGE;
    wsConfig.apiKey = "secret-api-key-12345";

    SigmaProtocol *WS = new SigmaWsClient(name, wsConfig);

    espErr = esp_event_handler_instance_register_with(
        SigmaProtocol::GetEventLoop(),
        name.c_str(),
        ESP_EVENT_ANY_ID,
        protocolEventHandler,
        WS,
        NULL);
    if (espErr != ESP_OK)
    {
      Log->Printf("Failed to register event handler: %d", espErr).Internal();
      exit(1);
    }
    WS->Connect();
    // Start delayed messaging
    TestDelayedMessaging(name, SigmaProtocol::GetEventLoop());
  }
#endif

#if PROTO_WS_SERVER == 1
  {
    WSServerConfig wsServerConfig;
    wsServerConfig.port = 8080;
    wsServerConfig.rootPath = "/";
    wsServerConfig.authType = AUTH_TYPE_FIRST_MESSAGE;
    wsServerConfig.apiKey = "secret-api-key-12345";
    SigmaProtocol *WS = new SigmaWsServer(name, wsServerConfig);
  }

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
  Transfer->AddProtocol("MQTT", Mqtt);
#endif
  network->Connect();
}

void loop()
{
  vTaskDelete(NULL);
}
