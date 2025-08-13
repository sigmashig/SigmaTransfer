#include "SigmaEthernet.h"
#include <SigmaAsyncNetwork.h>

SigmaEthernet::SigmaEthernet(EthernetConfig config, SigmaLoger *log)
{
    Log = log != nullptr ? log : new SigmaLoger(0);
    this->config = config;
    if (config.hardwareType == ETHERNET_HARDWARE_TYPE_W5500)
    {
        Ethernet.init(config.hardware.w5500.csPin);
        if (config.hardware.w5500.rstPin >= 0)
        {
            Log->Append("Resetting Ethernet").Internal();
            pinMode(config.hardware.w5500.rstPin, OUTPUT);
            digitalWrite(config.hardware.w5500.rstPin, LOW);
            delay(1);
            digitalWrite(config.hardware.w5500.rstPin, HIGH);
            delay(150);
        }
        // Configure optional INT pin if provided
        if (config.hardware.w5500.intPin >= 0)
        {
            pinMode(config.hardware.w5500.intPin, INPUT_PULLUP);
// Only some cores/libraries expose usingInterrupt on Ethernet
#if defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
            Ethernet.usingInterrupt(digitalPinToInterrupt(config.hardware.w5500.intPin));
#endif
        }
    }
    int result = 0;
    Log->Append("[START]Local IP: ").Append(Ethernet.localIP().toString()).Internal();
    /*
    if (config.dhcp)
    {
        Log->Append("Ethernet DHCP").Internal();
        Log->Printf("Ethernet MAC: %02X:%02X:%02X:%02X:%02X:%02X", config.mac[0], config.mac[1], config.mac[2], config.mac[3], config.mac[4], config.mac[5]).Internal();
        result = Ethernet.begin(config.mac);
        Log->Append("Ethernet DHCP result: ").Append(result).Internal();
    }
    if (result != 1)
    {
        Log->Append("Ethernet static").Internal();
        Ethernet.begin(config.mac, config.ip, config.dns, config.gateway, config.subnet);
    }
    // Give Network Module time to initialize
    delay(1000);
    Log->Append("Ethernet link status: ").Append(Ethernet.linkStatus()).Internal();
    // Check if ethernet is connected and get IP address
    if (Ethernet.linkStatus() == LinkON)
    {
        isConnected = true;
        //Log->Append("Ethernet connected: ").Append(Ethernet.localIP().toString()).Internal();
        String msg = "Ethernet connected: " + Ethernet.localIP().toString();
        Log->Append(msg).Internal();
        SigmaAsyncNetwork::PostEvent(NETWORK_ETHERNET_CONNECTED, (void *)msg.c_str(), msg.length() + 1);
    }
    else
    {
        Log->Append("Ethernet cable not connected!").Error();
        isConnected = false;
    }
    */
}

SigmaEthernet::~SigmaEthernet()
{
}

void SigmaEthernet::Connect()
{

    int result = 0;
    if (config.dhcp)
    {
        Log->Append("Ethernet DHCP").Internal();
        Log->Printf("Ethernet MAC: %02X:%02X:%02X:%02X:%02X:%02X", config.mac[0], config.mac[1], config.mac[2], config.mac[3], config.mac[4], config.mac[5]).Internal();
        result = Ethernet.begin(config.mac);
        Log->Append("Ethernet DHCP result: ").Append(result).Internal();
    }
    if (result != 1)
    {
        Log->Append("Ethernet static").Internal();
        Ethernet.begin(config.mac, config.ip, config.dns, config.gateway, config.subnet);
    }
    // Give Network Module time to initialize
    delay(1000);
    Log->Append("Ethernet link status: ").Append(Ethernet.linkStatus()).Internal();
    // Check if ethernet is connected and get IP address
    if (Ethernet.linkStatus() == LinkON)
    {
        isConnected = true;
        // Log->Append("Ethernet connected: ").Append(Ethernet.localIP().toString()).Internal();
        String msg = "Ethernet connected: " + Ethernet.localIP().toString();
        Log->Append(msg).Internal();
        SigmaAsyncNetwork::PostEvent(NETWORK_ETHERNET_CONNECTED, (void *)msg.c_str(), msg.length() + 1);
    }
    else
    {
        Log->Append("Ethernet cable not connected!").Error();
        isConnected = false;
    }
}

void SigmaEthernet::Disconnect()
{
    isConnected = false;
    SigmaAsyncNetwork::PostEvent(NETWORK_ETHERNET_DISCONNECTED, NULL, 0);
}

byte *SigmaEthernet::GenerateMac(byte id, byte mac[6])
{
    if (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 && mac[3] == 0 && mac[4] == 0 && mac[5] == 0)
    {
        mac[0] = 0x22;
        mac[1] = 0x33;
        mac[2] = 0x44;
        mac[3] = 0x55;
        mac[4] = 0x66;
    }
    mac[5] = id;
    return mac;
}
