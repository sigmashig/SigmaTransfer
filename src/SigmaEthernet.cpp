#include "SigmaEthernet.h"
#include "SigmaNetworkMgr.h"
#include "SigmaSPI.h"

extern "C"
{
#include "esp_log.h"
#include "driver/gpio.h"
}

SigmaEthernet::SigmaEthernet(const EthernetConfig &config, SigmaLoger *log) : config(config), SigmaNetwork(log)
{
}

SigmaEthernet::~SigmaEthernet()
{
    Disconnect();
}

bool SigmaEthernet::GetIpInfo(esp_netif_ip_info_t *out) const
{
    if (!netifHandle)
    {
        Log->Append("GetIpInfo: netifHandle is null").Error();
        return false;
    }
    return esp_netif_get_ip_info(netifHandle, out) == ESP_OK;
}

IPAddress SigmaEthernet::GetIpAddress() const
{
    esp_netif_ip_info_t ip_info;
    if (!GetIpInfo(&ip_info))
    {
        return IPAddress();
    }
    return IPAddress(ip_info.ip.addr);
}

void SigmaEthernet::handleEthEvent(int32_t id)
{
    switch (id)
    {
    case ETHERNET_EVENT_CONNECTED:
        isLinkUp = true;
        Log->Append("Ethernet Link Up").Internal();
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        isLinkUp = false;
        isGotIp = false;
        isConnected = false;
        Log->Append("Ethernet Link Down").Internal();
        SigmaNetworkMgr::PostEvent(NETWORK_ETHERNET_DISCONNECTED, (void *)"Ethernet Link is down", 0);
        break;
    case ETHERNET_EVENT_START:
        Log->Append("Ethernet Started").Internal();
        break;
    case ETHERNET_EVENT_STOP:
        Log->Append("Ethernet Stopped").Internal();
        isLinkUp = false;
        isGotIp = false;
        isConnected = false;
        SigmaNetworkMgr::PostEvent(NETWORK_ETHERNET_DISCONNECTED, (void *)"Ethernet stopped", 0);
        break;
    default:
        break;
    }
}

void SigmaEthernet::handleIpGot()
{
    isGotIp = true;
    isConnected = true;
    IPAddress ip = GetIpAddress();
    Log->Append("Ethernet connected. IP address:").Append(ip.toString()).Info();
    SigmaNetworkMgr::PostEvent(NETWORK_ETHERNET_CONNECTED, &ip, sizeof(ip));
}

void SigmaEthernet::onEthEvent(void *arg, esp_event_base_t, int32_t id, void *)
{
    reinterpret_cast<SigmaEthernet *>(arg)->handleEthEvent(id);
}

void SigmaEthernet::onIpEvent(void *arg, esp_event_base_t, int32_t, void *)
{
    reinterpret_cast<SigmaEthernet *>(arg)->handleIpGot();
}

bool SigmaEthernet::resetBoard()
{
    if (EthernetHardwareType::ETHERNET_HARDWARE_TYPE_W5500 == config.hardwareType)
    {
        config.hardware.w5500.ResetBoard();
    }
    return true;
}

bool SigmaEthernet::initBoard(spi_device_handle_t spiHandle)
{
    esp_err_t err = ESP_OK;
    if (EthernetHardwareType::ETHERNET_HARDWARE_TYPE_W5500 == config.hardwareType)
    {
        // Log->Append("Initializing W5500").Internal();
        eth_w5500_config_t w5500Config = ETH_W5500_DEFAULT_CONFIG(spiHandle);
        w5500Config.int_gpio_num = config.hardware.w5500.intPin;

        eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
        eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
        phy_config.reset_gpio_num = config.hardware.w5500.rstPin;

        esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500Config, &mac_config);
        esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

        esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
        err = esp_eth_driver_install(&eth_config, &ethHandle);
        if (err != ESP_OK)
        {
            Log->Append("[Error] esp_eth_driver_install failed").Error();
            return false;
        }
        // Log->Append("W5500 initialized").Internal();
    }
    return true;
}

bool SigmaEthernet::Connect()
{
    esp_err_t err = ESP_OK;

    resetBoard();
    // Init netif and default loop once per app
    // Log->Append("Initializing netif and default loop").Internal();
    if (!isNetifInited)
    {
        err = esp_netif_init();
        if (err != ESP_OK)
        {
            Log->Append("[Error] esp_netif_init failed").Error();
            return false;
        }
        err = esp_event_loop_create_default();
        if (err != ESP_OK)
        {
            Log->Append("[Error] esp_event_loop_create_default failed").Error();
            return false;
        }
        isNetifInited = true;
    }

    // Log->Append("Creating netif").Internal();
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    netifHandle = esp_netif_new(&netif_cfg);
    if (!netifHandle)
    {
        Log->Append("[Error] esp_netif_new failed").Error();
        return false;
    }
    // Log->Append("netif created").Internal();

    //--------------------------------

    spi_device_handle_t spiHandle = nullptr;
    err = SigmaSPI::Initialize(config.hardware.w5500.spiConfig, &spiHandle);
    if (err != ESP_OK)
    {
        Log->Append("[Error] SPI initialization failed").Error();
        return false;
    }
    // Log->Append("SPI initialized").Internal();
    err = gpio_install_isr_service(SigmaEthernet::GPIO_ISR_FLAGS);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        Log->Append("[Error] gpio_install_isr_service failed").Error();
        return false;
    }
    // Log->Append("GPIO ISR service installed").Internal();

    if (!initBoard(spiHandle))
    {
        Log->Append("[Error] initBoard failed").Error();
        return false;
    }

    // Log->Append("Setting MAC").Internal();
    //  Set MAC
    err = esp_eth_ioctl(ethHandle, ETH_CMD_S_MAC_ADDR, config.mac);
    if (err != ESP_OK)
    {
        Log->Append("[Error] esp_eth_ioctl failed").Error();
        return false;
    }

    // Attach netif glue
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(ethHandle);
    err = esp_netif_attach(netifHandle, glue);
    if (err != ESP_OK)
    {
        Log->Append("[Error] esp_netif_attach failed").Error();
        return false;
    }
    // Register events
    err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &SigmaEthernet::onEthEvent, this);
    if (err != ESP_OK)
    {
        Log->Append("[Error] esp_event_handler_register failed").Error();
        return false;
    }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &SigmaEthernet::onIpEvent, this);
    if (err != ESP_OK)
    {
        Log->Append("[Error] esp_event_handler_register failed").Error();
        return false;
    }
    // DHCP or static IP
    if (config.dhcp)
    {
        err = esp_netif_dhcpc_start(netifHandle);
        if (err != ESP_OK)
        {
            Log->Append("[Error] esp_netif_dhcpc_start failed").Error();
        }
    }
    if (!config.dhcp || err != ESP_OK)
    { // static IP
        err = esp_netif_dhcpc_stop(netifHandle);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
        {
            Log->Append("[Error] esp_netif_dhcpc_stop failed").Error();
        }
        esp_netif_ip_info_t ip_info = {};
        char buf[16];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", config.ip[0], config.ip[1], config.ip[2], config.ip[3]);
        err = esp_netif_str_to_ip4(buf, &ip_info.ip);
        if (err != ESP_OK)
        {
            Log->Append("[Error] IP Address is wrong:").Append(buf).Error();
        }
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", config.gateway[0], config.gateway[1], config.gateway[2], config.gateway[3]);
        err = esp_netif_str_to_ip4(buf, &ip_info.gw);
        if (err != ESP_OK)
        {
            Log->Append("[Error] Gateway Address is wrong:").Append(buf).Error();
        }
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", config.subnet[0], config.subnet[1], config.subnet[2], config.subnet[3]);
        err = esp_netif_str_to_ip4(buf, &ip_info.netmask);
        if (err != ESP_OK)
        {
            Log->Append("[Error] Subnet Address is wrong:").Append(buf).Error();
        }
        err = esp_netif_set_ip_info(netifHandle, &ip_info);

        esp_netif_dns_info_t dns_info = {};
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", config.dns[0], config.dns[1], config.dns[2], config.dns[3]);
        err = esp_netif_str_to_ip4(buf, &dns_info.ip.u_addr.ip4);
        if (err != ESP_OK)
        {
            Log->Append("[Error] DNS Address is wrong:").Append(buf).Error();
        }
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        err = esp_netif_set_dns_info(netifHandle, ESP_NETIF_DNS_MAIN, &dns_info);
        if (err != ESP_OK)
        {
            Log->Append("[Error] esp_netif_set_dns_info failed").Error();
        }
    }
    err = esp_eth_start(ethHandle);
    if (err != ESP_OK)
    {
        Log->Append("[Error] esp_eth_start failed").Error();
        return false;
    }
    Log->Append("Ethernet started").Internal();

    return true;
}

bool SigmaEthernet::Disconnect()
{
    if (ethHandle)
    {
        esp_eth_stop(ethHandle);
    }
    return true;
}
