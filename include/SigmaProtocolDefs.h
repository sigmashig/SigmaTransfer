#ifndef SIGMAPROTOCOLDEFS_H
#define SIGMAPROTOCOLDEFS_H

#pragma once

enum
{
    PROTOCOL_CONNECTED = 0x1000,
    PROTOCOL_DISCONNECTED,
    PROTOCOL_MESSAGE,
    PROTOCOL_ERROR,
} EVENT_IDS;

typedef struct
{
    String topic;
    int32_t eventId = 0;
    bool isReSubscribe = true;
} TopicSubscription;

#endif