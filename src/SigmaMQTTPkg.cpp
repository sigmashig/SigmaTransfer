#include "SigmaMQTTPkg.h"

SigmaMQTTPkg::SigmaMQTTPkg(String topic, String payload)
{
    this->topic = topic;
    this->payload = payload;
    int len = topic.length() + payload.length() + 2;
this->msg = (char *)malloc(len);
    strcpy(this->msg, topic.c_str());
    this->msg[topic.length()] = MQTTPKG_SEPARATOR;
    strcpy(this->msg + topic.length() + 1, payload.c_str());
    this->msg[len - 1] = '\0';
}

SigmaMQTTPkg::SigmaMQTTPkg(const char *msg)
{
    this->msg = (char *)malloc(strlen(msg) + 1);
    strcpy(this->msg, msg);
    this->msg[strlen(msg)] = '\0';
    char *separator = strchr(this->msg, MQTTPKG_SEPARATOR);
    if (separator != NULL)
    {
        *separator = '\0';
        this->topic = String(this->msg, separator - this->msg);
        this->payload = String(separator + 1, strlen(separator + 1));
    }
}

SigmaMQTTPkg::~SigmaMQTTPkg()
{
    free(this->msg);
}
