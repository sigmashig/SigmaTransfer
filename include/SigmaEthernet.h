#pragma once

#include <Arduino.h>

extern "C"
{
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_eth_netif_glue.h"
#include "driver/spi_master.h"
#include "esp_event.h"
}

#include "SigmaTransferDefs.h"
#include "SigmaNetwork.h"




class SigmaEthernet : public SigmaNetwork
{
public:
    SigmaEthernet(const EthernetConfig &config, SigmaLoger *log = nullptr);
    ~SigmaEthernet();

    static constexpr int GPIO_ISR_FLAGS = 0; // default flags for gpio_install_isr_service
    // Reset timings
    static constexpr uint32_t RESET_PULSE_MS = 10;
    static constexpr uint32_t RESET_POST_DELAY_MS = 50;

    bool Connect() override;
    bool Disconnect() override;

    bool gotIp() const { return isGotIp; }
    bool linkUp() const { return isLinkUp; }
    esp_netif_t *netif() const { return netifHandle; }
    bool GetIpInfo(esp_netif_ip_info_t *out) const;
    IPAddress GetIpAddress() const;
    

private:
    EthernetConfig config;
    esp_netif_t *netifHandle = nullptr;
    esp_eth_handle_t ethHandle = nullptr;
    bool isNetifInited = false;
    //bool isLinkUp = false;
    //bool isGotIp = false;

    bool resetBoard();
    bool initBoard(spi_device_handle_t spiHandle);

    
    static void onEthEvent(void *arg, esp_event_base_t base, int32_t id, void *data);
    static void onIpEvent(void *arg, esp_event_base_t base, int32_t id, void *data);
    void handleEthEvent(int32_t id);
    void handleIpGot();

};
