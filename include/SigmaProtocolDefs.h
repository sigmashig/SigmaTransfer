#ifndef SIGMAPROTOCOLDEFS_H
#define SIGMAPROTOCOLDEFS_H

#pragma once

enum
{
    PROTOCOL_CONNECTED = 0x1000,
    PROTOCOL_DISCONNECTED,
    PROTOCOL_MESSAGE,
    PROTOCOL_ERROR,
    PROTOCOL_PING,
    PROTOCOL_PONG,
} EVENT_IDS;

typedef struct
{
    String topic;
    int32_t eventId = 0;
    bool isReSubscribe = true;
} TopicSubscription;

typedef struct
{
    String protocolName;
    String topic;
    String payload;
} Message;

#endif