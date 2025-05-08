#include <Arduino.h>
#include <SigmaTransfer.h>
#include <SigmaMQTT.h>
#include <SigmaChannel.h>

SigmaTransfer *Transfer;

enum {
  EVENT_TEST1 = 0x80,
  EVENT_TEST2 = 0x81,
  EVENT_TEST3 = 0x82,
  EVENT_TEST4 = 0x83,
  EVENT_TEST5 = 0x84
} MY_EVENT_IDS;


void setup() {
  Serial.begin(115200);
  Serial.println("----Hello, World!-----");

  // init protocols
  Transfer = new SigmaTransfer("Sigma", "kybwynyd");
  // Add MQTT
  MqttConfig mqttConfig;
  mqttConfig.server = "192.168.0.102";
  
  SigmaProtocol *Mqtt = new SigmaMQTT(mqttConfig);
  Transfer->AddProtocol("MQTT", Mqtt);
  
// Add Channels
  SigmaChannelConfig channelConfig1;
  channelConfig1.name = "Channel_1";
  channelConfig1.protocol = Mqtt;
  channelConfig1.rootTopic = "test/test1/";
  channelConfig1.crypt = NULL;


  SigmaChannel *Channel = new SigmaChannel(channelConfig1);
  Transfer->AddChannel(Channel);

  SigmaChannelConfig channelConfig2;
  channelConfig2.name = "Channel_2";
  channelConfig2.protocol = Mqtt;
  channelConfig2.rootTopic = "test/test2/";
  channelConfig2.crypt = NULL;

  SigmaChannel *Channel2 = new SigmaChannel(channelConfig2);
  Transfer->AddChannel(Channel2);

  // Subscribe to channel topics
  TopicSubscription subscription;
  subscription.topic = "t3";
  subscription.eventId = EVENT_TEST1;
  subscription.isReSubscribe = true;
  Channel->Subscribe(subscription);

  TopicSubscription subscription2;
  subscription2.topic = "t4";
  subscription2.eventId = EVENT_TEST2;
  subscription2.isReSubscribe = true;
  Channel2->Subscribe(subscription2);

  // Start



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
  Serial.println("Sending message");
  Channel->Send("t3","Hello, World!");
  Channel2->Send("t3","Hello, World!");


  }

void loop() {
  vTaskDelete(NULL);
}
