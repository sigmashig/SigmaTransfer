#include <Arduino.h>
#include <SigmaTransfer.h>
#include <SigmaMQTT.h>
#include <SigmaChannel.h>

SigmaTransfer *Transfer;
SigmaChannel *Channel;
SigmaChannel *Channel2;

enum {
  EVENT_TEST1 = 0x80,
  EVENT_TEST2 = 0x81,
  EVENT_TEST3 = 0x82,
  EVENT_TEST4 = 0x83,
  EVENT_TEST5 = 0x84
} MY_EVENT_IDS;


void TestMqtt()
{
    Serial.println("Sending message");

  Channel->Send("t3","Hello, World!");
  Channel2->Send("t4","Hello, World!");

}

void protocolEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  Serial.println(" protocolEventHandler ");
  Log->Append("Protocol event: ").Append(event_id).Internal();
  if (strcmp(SIGMAMQTT_EVENT, event_base) == 0)
  {

    if (event_id == PROTOCOL_CONNECTED)
    {
      String name = (char *)event_data;
      Log->Append("MQTT connected: ").Append(name).Internal();
      TestMqtt();
    }
    else if (event_id == EVENT_TEST1)
    {
      SigmaMQTTPkg pkg = SigmaMQTTPkg((char *)event_data);
      Log->Append("EVENT_TEST1:[").Append(pkg.GetTopic()).Append("]:").Append(pkg.GetPayload()).Internal();
    }
    else if (event_id == EVENT_TEST2)
    {
      SigmaMQTTPkg pkg = SigmaMQTTPkg((char *)event_data);
      Log->Append("EVENT_TEST2:[").Append(pkg.GetTopic()).Append("]:").Append(pkg.GetPayload()).Internal();
    }
    else if (event_id == PROTOCOL_DISCONNECTED)
    {
      String name = (char *)event_data;
      Log->Append("MQTT disconnected: ").Append(name).Internal();
    }
    else if (event_id == PROTOCOL_MESSAGE)
    {
      SigmaMQTTPkg pkg = SigmaMQTTPkg((char *)event_data);
      Log->Append("SIGMAMQTT_MESSAGE:[").Append(pkg.GetTopic()).Append("]:").Append(pkg.GetPayload()).Internal();
    }

    else
    {
      Log->Internal("Unknown event");
    }
  }
}



void setup() {
  Serial.begin(115200);
  Serial.println("----Hello, World!-----");
  Log = new SigmaLoger(512);
  esp_err_t espErr = ESP_OK;

  esp_event_loop_create_default();
  if (espErr != ESP_OK && espErr != ESP_ERR_INVALID_STATE)
  {
    Log->Printf(F("Failed to create default event loop: %d"), espErr).Internal();
  }

  delay(100);
  

  espErr = esp_event_handler_instance_register(SIGMAMQTT_EVENT,
                                            ESP_EVENT_ANY_ID,
                                            protocolEventHandler,
                                            NULL,
                                            NULL);
  if (espErr != ESP_OK)
  {
    Log->Printf("Failed to register event handler: %d", espErr).Internal();
  }


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


  Channel = new SigmaChannel(channelConfig1);
  Transfer->AddChannel(Channel);

  SigmaChannelConfig channelConfig2;
  channelConfig2.name = "Channel_2";
  channelConfig2.protocol = Mqtt;
  channelConfig2.rootTopic = "test/test2/";
  channelConfig2.crypt = NULL;

  Channel2 = new SigmaChannel(channelConfig2);
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


  }

void loop() {
  vTaskDelete(NULL);
}
