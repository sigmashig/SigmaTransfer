#include "SigmaUART.h"
#include <esp_event.h>
#include "SigmaInternalPkg.h"


//ESP_EVENT_DEFINE_BASE(SigmaUART_EVENT);
ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);


SigmaUART::SigmaUART(UartConfig config)
{
    SetParams(config);
    eventMap.clear();
    serial = new HardwareSerial(config.uartNum);
    serial->onReceive(onReceiveMsg);
    serial->onReceiveError(onReceiveError);
}

void SigmaUART::SetParams(UartConfig config)
{
    this->config = config;   
}

SigmaUART::SigmaUART()
{
}

bool SigmaUART::BeginSetup()
{
//    serial->begin(config.baudRate,config.portConfig,config.rxPin,config.txPin);
    return true;
}

bool SigmaUART::FinalizeSetup()
{
    return true;
}



void SigmaUART::Subscribe(TopicSubscription subscriptionTopic, String rootTopic)
{
    if (rootTopic != "")
    {
        subscriptionTopic.topic = rootTopic + subscriptionTopic.topic;
    }
    eventMap[subscriptionTopic.topic] = subscriptionTopic;
}

void SigmaUART::Publish(String topic, String payload)
{
    SigmaInternalPkg pkg(topic,payload);
    serial->write(pkg.GetMsg(),pkg.GetMsgLength());
}

void SigmaUART::Unsubscribe(String topic, String rootTopic)
{
    if (rootTopic != "")
    {
        topic = rootTopic + topic;
    }
    eventMap.erase(topic);
}
void SigmaUART::Connect()
{
    serial->begin(config.baudRate,config.portConfig,config.rxPin,config.txPin);
    isConnected = true;
}
void SigmaUART::Disconnect()
{
    serial->end();
    isConnected = false;
}

void SigmaUART::onReceiveMsg()
{
    String msg = "";
    while (serial->available()) {
        char c = serial->read();
        msg += c;
        if (c == INTERNALPKG_MESSAGE_END) {
            SigmaInternalPkg pkg(msg);
            for (auto const &x : eventMap)
            {
                if (pkg.GetTopic() == x.first)
                {
                    int32_t e = (x.second.eventId == 0 ? PROTOCOL_MESSAGE : x.second.eventId);
                    esp_err_t res = esp_event_post(SIGMATRANSFER_EVENT, e, pkg.GetMsg(), strlen(pkg.GetMsg()) + 1, portMAX_DELAY);
                }
            }
            esp_event_post(SIGMATRANSFER_EVENT, PROTOCOL_MESSAGE, (void *)(pkg.GetMsg()), pkg.GetMsgLength(), portMAX_DELAY);
            msg = "";
        }
    }
}

void SigmaUART::onReceiveError(hardwareSerial_error_t error)
{
    esp_err_t res = esp_event_post(SIGMATRANSFER_EVENT, PROTOCOL_ERROR, (void*) error, sizeof(error), portMAX_DELAY);
    
}
