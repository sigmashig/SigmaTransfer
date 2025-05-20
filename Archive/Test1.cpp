#include <Arduino.h>
#include <SigmaTransfer.h>
#include <SigmaUART.h>
#include <SigmaChannel.h>
#include <SigmaMQTT.h>

// SigmaTransfer *Transfer;
// esp_event_loop_handle_t SigmaTransfer_event_loop = nullptr;
#define PROTO_MQTT 0
#define PROTO_UART 0
#define PROTO_WS 1
enum
{
  EVENT_TEST1 = 0x80,
  EVENT_TEST2 = 0x81,
  EVENT_TEST3 = 0x82,
  EVENT_TEST4 = 0x83,
  EVENT_TEST5 = 0x84
} MY_EVENT_IDS;
void TestDelayedMessaging(String name)
{
  SigmaProtocol *protocol = Transfer->GetProtocol(name);
  if (protocol != NULL)
  {
    protocol->Send("delayedTopic1", "Hello, World!");
  }
}

void TestMessaging(String name)
{
  SigmaProtocol *protocol = Transfer->GetProtocol(name);
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

    protocol->Send("testTopic1", "Hello, World!");
    protocol->Send("testTopic2", "Hello, World!");
  }
}

void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  Serial.println(" protocolEventHandler ");
  Log->Append("Protocol event: ").Append(event_id).Internal();
  if (strcmp(SIGMATRANSFER_EVENT, event_base) == 0)
  {

    if (event_id == PROTOCOL_CONNECTED)
    {
      String name = (char *)event_data;
      Log->Append("Protocol connected: ").Append(name).Internal();
      TestMessaging(name);
    }
    else if (event_id == EVENT_TEST1)
    {
      SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
      Log->Append("EVENT_TEST1:[").Append(pkg.GetTopic()).Append("]:").Append(pkg.GetPayload()).Internal();
    }
    else if (event_id == EVENT_TEST2)
    {
      SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
      Log->Append("EVENT_TEST2:[").Append(pkg.GetTopic()).Append("]:").Append(pkg.GetPayload()).Internal();
    }
    else if (event_id == PROTOCOL_DISCONNECTED)
    {
      String name = (char *)event_data;
      Log->Append("Protocol disconnected: ").Append(name).Internal();
    }
    else if (event_id == PROTOCOL_MESSAGE)
    {
      SigmaInternalPkg pkg = SigmaInternalPkg((char *)event_data);
      Log->Append("SIGMA_TRANSFER_MESSAGE:[").Append(pkg.GetTopic()).Append("]:").Append(pkg.GetPayload()).Internal();
    }

    else
    {
      Log->Internal("Unknown event");
    }
  }
}

void setup()
{
  Serial.begin(115200);
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

    espErr = esp_event_loop_create(&loop_args, &SigmaTransfer_event_loop);
    if (espErr != ESP_OK)
    {
      Log->Printf("Failed to create event loop: %d", espErr).Internal();
      exit(1);
    }
  */
}

// init protocols
SigmaTransfer *Transfer = new SigmaTransfer("Sigma", "kybwynyd");

#if PROTO_WS == 1
WSConfig wsConfig;
wsConfig.host = "192.168.0.102";
wsConfig.port = 8080;
wsConfig.rootTopic = "test/test1/";
wsConfig.username = "test";
wsConfig.password = "password";
SigmaProtocol *WS = new SigmaWS("WS", wsConfig);
Transfer->AddProtocol("WS", WS);

espErr = esp_event_handler_instance_register_with(
    WS->GetEventLoop(),
    SIGMATRANSFER_EVENT,
    ESP_EVENT_ANY_ID,
    protocolEventHandler,
    NULL,
    NULL);
if (espErr != ESP_OK)
{
  Log->Printf("Failed to register event handler: %d", espErr).Internal();
  exit(1);

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
  // Start delayed messaging
#ifdef PROTO_MQTT
  TestDelayedMessaging("MQTT");
#endif

  if (Transfer->Begin())
  {
    Serial.println("Transfer initialized");
  }
  else
  {
    Serial.println("Transfer not initialized");
    exit(1);
  }

  // Init WiFi

  // Send message
}

void loop()
{
  vTaskDelete(NULL);
}
