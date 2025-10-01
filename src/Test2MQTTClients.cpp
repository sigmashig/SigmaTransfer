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
    NetworkConfig networkConfig;
    networkConfig.wifiConfig.wifiSta.ssid = "Sigma_Guest";
    networkConfig.wifiConfig.wifiSta.password = "Passwd#123";
    networkConfig.wifiConfig.wifiMode = WIFI_MODE_STA;
    networkConfig.wifiConfig.enabled = true;
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
    SigmaNetworkMgr *network = new SigmaNetworkMgr(networkConfig, Log);
  
    SigmaConnectionsConfig config;
  
  
    config.mqttConfig.server = "192.168.0.102";
    config.mqttConfig.rootTopic = "test";
    config.mqttConfig.username = "test";
    config.mqttConfig.password = "password";
    config.mqttConfig.networkMode = NETWORK_MODE_LAN;
    
   
    Log->Append("Creating MQTT").Internal();
    SigmaMQTT *MqttA = (SigmaMQTT *)SigmaConnection::Create(SigmaProtocolType::SIGMA_PROTOCOL_MQTT, config, Log);
    if (MqttA == NULL)
    {
      Log->Append("MQTT A creation failed").Internal();
      return;
    }
    Log->Append("Connections created").Internal();
    
    espErr = MqttA->RegisterEventHandlers(ESP_EVENT_ANY_ID, protocolEventHandler, MqttA);
    if (espErr != ESP_OK)
    {
      Log->Printf("Failed to register event handler: %d", espErr).Internal();
    }
    Log->Append("event handler registered").Internal();
    Log->Append("Subscribing to testListen").Internal();
    TopicSubscription subscription;
    subscription.topic = "testListen";
    subscription.isReSubscribe = true;
    MqttA->Subscribe(subscription);
    Log->Append("Connecting to network").Internal();
    network->Connect();
    // delay(10000);
    // network->GetEthernet()->Connect();
    Log->Append("Waiting for protocol to be ready").Internal();
    while (MqttA->IsReady() == false)
    {
      delay(100);
    }
    Log->Append("MQTT A is ready").Internal();
    Log->Append("Setup end").Internal();
  

}

void loop()
{
    vTaskDelete(NULL);
}