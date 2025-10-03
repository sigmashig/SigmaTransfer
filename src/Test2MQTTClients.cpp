#include <Arduino.h>
#include <SigmaLoger.h>
#include <SigmaNetworkMgr.h>
#include <SigmaMQTT.h>

SigmaLoger *Log;
SigmaMQTT *MqttA;
SigmaMQTT *MqttB;

void SendTestMessage(SigmaConnection *protocol)
{
  SigmaInternalPkg pkg("test1", "Hello, World!");
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
    Log->Append("RECEIVED_MESSAGE:[").Append(pkg.GetTopic()).Append("]#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else if (event_id == PROTOCOL_DISCONNECTED)
  {
    Log->Append("PROTOCOL_DISCONNECTED").Internal();
  }
  else if (event_id == PROTOCOL_CONNECTED)
  {
    char *msg = (char *)event_data;
    Log->Append("PROTOCOL_CONNECTED:").Append(msg).Append("#").Internal();
    SendTestMessage(protocol);
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
    Log->Append("SEND_MESSAGE:[").Append(pkg.GetTopic()).Append("]#").Append(pkg.GetPayload()).Append("#").Internal();
  }
  else
  {
    Log->Internal("Unknown event");
    Log->Append("Protocol:").Append(protocol->GetName()).Append(" Base:").Append(event_base).Append("#ID: ").Append(event_id).Internal();
  }
  Log->Append("Protocol event end").Internal();
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("\n----Hello, World!-----");

  Log = new SigmaLoger(512);

  esp_err_t espErr = ESP_OK;
  NetworkConfig wifiConfig;
  wifiConfig.name = "wifi";
  wifiConfig.type = NETWORK_WIFI;
  wifiConfig.networkConfig.wifiConfig.wifiSta.ssid = "Sigma_Guest";
  wifiConfig.networkConfig.wifiConfig.wifiSta.password = "Passwd#123";
  wifiConfig.networkConfig.wifiConfig.wifiMode = WIFI_MODE_STA;
  wifiConfig.networkConfig.wifiConfig.enabled = true;
  wifiConfig.networkConfig.wifiConfig.wifiSta.useDhcp = false;
  wifiConfig.networkConfig.wifiConfig.wifiSta.ip = IPAddress(192, 168, 0, 222);
  wifiConfig.networkConfig.wifiConfig.wifiSta.dns = IPAddress(8, 8, 8, 8);
  wifiConfig.networkConfig.wifiConfig.wifiSta.gateway = IPAddress(192, 168, 0, 1);
  wifiConfig.networkConfig.wifiConfig.wifiSta.subnet = IPAddress(255, 255, 255, 0);

  NetworkConfig ethernetConfig;
  ethernetConfig.name = "ethernet";
  ethernetConfig.type = NETWORK_ETHERNET;
  ethernetConfig.networkConfig.ethernetConfig.enabled = true;
  ethernetConfig.networkConfig.ethernetConfig.hardwareType = ETHERNET_HARDWARE_TYPE_W5500;
  ethernetConfig.networkConfig.ethernetConfig.hardware.w5500.spiConfig.csPin = GPIO_NUM_5;
  ethernetConfig.networkConfig.ethernetConfig.hardware.w5500.rstPin = GPIO_NUM_4;
  ethernetConfig.networkConfig.ethernetConfig.hardware.w5500.intPin = GPIO_NUM_15;
  ethernetConfig.networkConfig.ethernetConfig.dhcp = true;
  ethernetConfig.networkConfig.ethernetConfig.ip = IPAddress(192, 168, 0, 222);
  ethernetConfig.networkConfig.ethernetConfig.dns = IPAddress(8, 8, 8, 8);
  ethernetConfig.networkConfig.ethernetConfig.gateway = IPAddress(192, 168, 0, 1);
  ethernetConfig.networkConfig.ethernetConfig.subnet = IPAddress(255, 255, 255, 0);
  //SigmaNetwork::GenerateMac(ethernetConfig.networkConfig.ethernetConfig.mac, 15);
  /*
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
  */
  SigmaNetworkMgr::SetLog(Log);
  std::vector<NetworkConfig> networks;
  networks.push_back(wifiConfig);
  networks.push_back(ethernetConfig);
  //if (!SigmaNetworkMgr::AddNetworks(networks))
  if(SigmaNetworkMgr::AddNetwork(wifiConfig) == nullptr)
  {
    Log->Append("Failed to add network").Internal();
    return;
  }

  SigmaConnectionsConfig configA;

  configA.mqttConfig.server = "192.168.0.102";
  configA.mqttConfig.rootTopic = "test";
  configA.mqttConfig.username = "test";
  configA.mqttConfig.password = "password";
  configA.mqttConfig.networkMode = NETWORK_MODE_LAN;
  configA.mqttConfig.clientId = "MqttA";

  Log->Append("Creating MQTTA").Internal();
  SigmaMQTT *MqttA = (SigmaMQTT *)SigmaConnection::Create(SigmaProtocolType::SIGMA_PROTOCOL_MQTT, configA, Log);
  if (MqttA == NULL)
  {
    Log->Append("MQTT A creation failed").Internal();
    return;
  }
  SigmaConnectionsConfig configB;

  configB.mqttConfig.server = "192.168.0.99";
  configB.mqttConfig.rootTopic = "test";
  configB.mqttConfig.username = "";
  configB.mqttConfig.password = "";
  configB.mqttConfig.networkMode = NETWORK_MODE_LAN;
  configB.mqttConfig.clientId = "MqttB";

  Log->Append("Creating MQTTB").Internal();
  SigmaMQTT *MqttB = (SigmaMQTT *)SigmaConnection::Create(SigmaProtocolType::SIGMA_PROTOCOL_MQTT, configB, Log);
  if (MqttB == NULL)
  {
    Log->Append("MQTT B creation failed").Internal();
    return;
  }


  Log->Append("Connections created").Internal();

  espErr = MqttA->RegisterEventHandlers(ESP_EVENT_ANY_ID, protocolEventHandler, MqttA);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register event handler (MqttA): %d", espErr).Internal();
  }
  espErr = MqttB->RegisterEventHandlers(ESP_EVENT_ANY_ID, protocolEventHandler, MqttB);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register event handler (MqttB): %d", espErr).Internal();
  }
  Log->Printf("MqttA: %p", MqttA).Internal();
  Log->Printf("MqttB: %p", MqttB).Internal();
  Log->Append("Subscribing to testListen").Internal();
  TopicSubscription subscription;
  subscription.topic = "testListen";
  subscription.isReSubscribe = true;
  MqttA->Subscribe(subscription);
  MqttB->Subscribe(subscription);
  Log->Append("Connecting to network").Internal();
  SigmaNetworkMgr::Connect();
  // delay(10000);
  // network->GetEthernet()->Connect();
  Log->Append("Waiting for protocol to be ready").Internal();
  while (MqttA->IsReady() == false)
  {
    delay(100);
  }
  Log->Append("MQTT A is ready").Internal();
  while (MqttB->IsReady() == false)
  {
    delay(100);
  }
  Log->Append("MQTT B is ready").Internal();
  Log->Append("Setup end").Internal();
}

void loop()
{
  vTaskDelete(NULL);
}