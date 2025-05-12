#include "SigmaInternalPkg.h"

SigmaInternalPkg::SigmaInternalPkg(String topic, String payload)
{
    this->topic = topic;
    this->payload = payload;
    this->msgLength = topic.length() + payload.length() + 2;
    this->msg = (char *)malloc(msgLength);
    strcpy(this->msg, topic.c_str());
    this->msg[topic.length()] = INTERNALPKG_TOPIC_SEPARATOR;
    strcpy(this->msg + topic.length() + 1, payload.c_str());
    this->msg[msgLength - 1] = INTERNALPKG_MESSAGE_END;
}


SigmaInternalPkg::SigmaInternalPkg(const char *msg)
{
    this->msg = (char *)malloc(strlen(msg) + 1);
    this->msgLength = strlen(msg) + 1;
    strcpy(this->msg, msg);
    this->msg[strlen(msg)] = INTERNALPKG_MESSAGE_END;
    char *separator = strchr(this->msg, INTERNALPKG_TOPIC_SEPARATOR);
    if (separator != NULL)
    {
        *separator = INTERNALPKG_MESSAGE_END;
        this->topic = String(this->msg, separator - this->msg);
        this->payload = String(separator + 1, strlen(separator + 1));
    }
}

SigmaInternalPkg::~SigmaInternalPkg()
{
    free(this->msg);
}
