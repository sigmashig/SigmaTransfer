#include <Arduino.h>
#include <SigmaMQTT.h>
#include <SigmaWsClient.h>
#include <SigmaNetworkMgr.h>
#include <SigmaWsServer.h>

#define PROTO_MQTT 1
#define PROTO_UART 0
#define PROTO_WS_CLIENT 0
#define PROTO_WS_SERVER 0

SigmaLoger *Log;

enum
{
  EVENT_TEST1 = 0x80,
  EVENT_TEST2 = 0x81,
  EVENT_TEST3 = 0x82,
  EVENT_TEST4 = 0x83,
  EVENT_TEST5 = 0x84
} MY_EVENT_IDS;

SigmaConnection *protocol = NULL;

void TestSuite1()
{
  Log->Append("TestSuite1").Internal();
  TopicSubscription subscription;
  // subscription for slave
  subscription.topic = "topic1";
  subscription.eventId = EVENT_TEST1;
  subscription.isReSubscribe = true;
  // Log->Printf("Protocol:%p#", protocol).Internal();
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
  protocol->PostMessageEvent(pkg.GetPkgString(), PROTOCOL_SEND_SIGMA_MESSAGE);
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
  networkConfig.ethernetConfig.hardwareType = ETHERNET_HARDWARE_TYPE_W5500;
  networkConfig.ethernetConfig.hardware.w5500.spiConfig.csPin = GPIO_NUM_5;
  networkConfig.ethernetConfig.hardware.w5500.rstPin = GPIO_NUM_4;
  networkConfig.ethernetConfig.hardware.w5500.intPin = GPIO_NUM_15;
  networkConfig.ethernetConfig.dhcp = true;
  networkConfig.ethernetConfig.ip = IPAddress(192, 168, 0, 222);
  networkConfig.ethernetConfig.dns = IPAddress(8, 8, 8, 8);
  networkConfig.ethernetConfig.gateway = IPAddress(192, 168, 0, 1);
  networkConfig.ethernetConfig.subnet = IPAddress(255, 255, 255, 0);
  SigmaNetwork::GenerateMac(networkConfig.ethernetConfig.mac, 15);

  SigmaNetworkMgr *network = new SigmaNetworkMgr(networkConfig, Log);
  Log->Append("Creating MQTT").Internal();
  MqttConfig mqttConfig;
  mqttConfig.server = "192.168.0.102";
  mqttConfig.rootTopic = "test/test1";
  mqttConfig.username = "test";
  mqttConfig.password = "password";
  mqttConfig.networkMode = NETWORK_MODE_LAN;
  SigmaMQTT *Mqtt = SigmaConnection::Create(SigmaProtocolType::SIGMA_PROTOCOL_MQTT, mqttConfig, Log);
  Log->Append("MQTT created").Internal();
  protocol = Mqtt;

  espErr = Mqtt->RegisterEventHandlers(ESP_EVENT_ANY_ID, protocolEventHandler, Mqtt);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register event handler: %d", espErr).Internal();
  }
  Log->Append("event handler registered").Internal();
  Log->Append("Connecting to network").Internal();
  network->Connect();
  // delay(10000);
  // network->GetEthernet()->Connect();
  Log->Append("Waiting for protocol to be ready").Internal();
  while (protocol->IsReady() == false)
  {
    delay(100);
  }
  Log->Append("Setup end").Internal();
}

void loop()
{
  vTaskDelete(NULL);
}
