#include "SigmaUART.h"
#include <esp_event.h>
#include "SigmaInternalPkg.h"

// ESP_EVENT_DEFINE_BASE(SigmaUART_EVENT);
ESP_EVENT_DECLARE_BASE(SIGMATRANSFER_EVENT);

SigmaUART::SigmaUART(UartConfig _config)
{
    SetParams(_config);
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

void SigmaUART::Subscribe(TopicSubscription subscriptionTopic)
{
    eventMap[subscriptionTopic.topic] = subscriptionTopic;
}

void SigmaUART::Publish(String topic, String payload)
{
    SigmaInternalPkg pkg(name, topic, payload);
    serial->write(pkg.GetPkgString().c_str());
}

void SigmaUART::Unsubscribe(String topic)
{
    eventMap.erase(topic);
}
void SigmaUART::Connect()
{
    serial->begin(config.baudRate, config.portConfig, config.rxPin, config.txPin);
    esp_event_post(SIGMATRANSFER_EVENT, PROTOCOL_CONNECTED, (void *)(name.c_str()), name.length() + 1, portMAX_DELAY);
    isConnected = true;
}
void SigmaUART::Disconnect()
{
    serial->end();
    isConnected = false;
    esp_event_post(SIGMATRANSFER_EVENT, PROTOCOL_DISCONNECTED, (void *)(name.c_str()), name.length() + 1, portMAX_DELAY);
}

void SigmaUART::onReceiveMsg()
{
    String msg = "";
    bool isBinary = false;
    int length = serial->available();
    byte *payload = (byte *)malloc(length);
    serial->readBytes(payload, length);
    PostEvent(PROTOCOL_MESSAGE, payload, length);

    for (int i = 0; i < length; i++)
    {
        if (!std::isprint(payload[i]))
        {
            isBinary = true;
            break;
        }
    }
    if (!isBinary) 
    {
    SigmaInternalPkg pkg(name, "", payload, length);
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

void SigmaUART::onReceiveError(hardwareSerial_error_t error)
{
    esp_err_t res = esp_event_post(SIGMATRANSFER_EVENT, PROTOCOL_ERROR, (void *)error, sizeof(error), portMAX_DELAY);
}
