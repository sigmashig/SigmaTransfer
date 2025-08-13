
#include "SigmaNetwork.h"

SigmaNetwork::SigmaNetwork(SigmaLoger *log)
{
    Log = log != nullptr ? log : new SigmaLoger(0);
}

SigmaNetwork::~SigmaNetwork()
{
}

void SigmaNetwork::GenerateMac(byte mac[6], byte id)
{
    mac[0] = 0xDE;
    mac[1] = 0xAD;
    mac[2] = 0xBE;
    mac[3] = 0xEF;
    mac[4] = 0x00;
    if (id == 0xFF)
    {
        mac[5] = (byte)random(0, 253);
    }
    else
    {
        mac[5] = id;
    }
}

String SigmaNetwork::MACToString(const byte mac[6])
{
    String buf = String(mac[0], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[2], HEX) + ":" + String(mac[3], HEX) + ":" + String(mac[4], HEX) + ":" + String(mac[5], HEX);
    return String(buf);
}
