#include "SigmaEthernet.h"
#include "SigmaNetworkMgr.h"
#include "SigmaSPI.h"

SigmaEthernet::SigmaEthernet(const EthernetConfig &config) : config(config)
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
        SigmaNetworkMgr::GetLog()->Append("GetIpInfo: netifHandle is null").Error();
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
    {
        isLinkUp = true;
        SigmaNetworkMgr::GetLog()->Append("Ethernet Link Up").Internal();
        IPAddress ip = GetIpAddress();
        SigmaNetworkMgr::GetLog()->Append("IP address:").Append(ip.toString()).Info();
        break;
    }
    case ETHERNET_EVENT_DISCONNECTED:
    {
        isLinkUp = false;
        isGotIp = false;
        isConnected = false;
        String msg = "Ethernet Link is down";
        SigmaNetworkMgr::GetLog()->Append(msg).Internal();
        SigmaNetworkMgr::PostEvent(NETWORK_ETHERNET_DISCONNECTED, (void *)msg.c_str(), msg.length() + 1);
        break;
    }
    case ETHERNET_EVENT_START:
        SigmaNetworkMgr::GetLog()->Append("Ethernet Started").Internal();
        break;
    case ETHERNET_EVENT_STOP:
    {
        SigmaNetworkMgr::GetLog()->Append("Ethernet Stopped").Internal();
        isLinkUp = false;
        isGotIp = false;
        isConnected = false;
        String msg = "Ethernet stopped";
        SigmaNetworkMgr::PostEvent(NETWORK_ETHERNET_DISCONNECTED, (void *)msg.c_str(), msg.length() + 1);
        break;
    }
    default:
        break;
    }
}

void SigmaEthernet::handleIpGot()
{
    isGotIp = true;
    isConnected = true;
    IPAddress ip = GetIpAddress();
    SigmaNetworkMgr::GetLog()->Append("Ethernet connected. IP address:").Append(ip.toString()).Info();
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
        SigmaNetworkMgr::GetLog()->Append("Initializing W5500").Internal();
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
            SigmaNetworkMgr::GetLog()->Append("[Error] esp_eth_driver_install failed").Error();
            return false;
        }
        // Log->Append("W5500 initialized").Internal();
    }
    return true;
}

bool SigmaEthernet::Begin()
{
    if (!config.enabled)
    {
        SigmaNetworkMgr::GetLog()->Append("Ethernet is disabled").Internal();
        return true;
    }
    SigmaNetworkMgr::Init(); // init event loop and netif
    esp_err_t err = ESP_OK;
    SigmaNetworkMgr::GetLog()->Append("Begin Ethernet").Internal();
    if (!resetBoard())
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] resetBoard failed").Error();
        return false;
    }
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    netifHandle = esp_netif_new(&netif_cfg);
    if (!netifHandle)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] esp_netif_new failed").Error();
        return false;
    }

    //--------------------------------

    spi_device_handle_t spiHandle = nullptr;
    err = SigmaSPI::Initialize(config.hardware.w5500.spiConfig, &spiHandle);
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] SPI initialization failed").Error();
        return false;
    }
    // Log->Append("SPI initialized").Internal();
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] gpio_install_isr_service failed").Error();
        return false;
    }
    // Log->Append("GPIO ISR service installed").Internal();

    if (!initBoard(spiHandle))
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] initBoard failed").Error();
        return false;
    }

    // Log->Append("Setting MAC").Internal();
    //  Set MAC
    if (config.mac[0] == 0 && config.mac[1] == 0 && config.mac[2] == 0 && config.mac[3] == 0 && config.mac[4] == 0 && config.mac[5] == 0)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] MAC address is not set").Error();
        GenerateMac(config.mac);
        //return false;
    }
    err = esp_eth_ioctl(ethHandle, ETH_CMD_S_MAC_ADDR, config.mac);
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] esp_eth_ioctl failed").Error();
        return false;
    }

    // Attach netif glue
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(ethHandle);
    if (glue == nullptr)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] esp_eth_new_netif_glue failed").Error();
        return false;
    }
    err = esp_netif_attach(netifHandle, glue);
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] esp_netif_attach failed").Error();
        return false;
    }
    /*
    // Register default Ethernet handlers so esp-netif reacts to ETH events (link up/down)
    err = esp_eth_set_default_handlers(netifHandle);
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] esp_eth_set_default_handlers failed").Error();
        return false;
    }
    */
    // Register events
    err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, onEthEvent, this);
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] esp_event_handler_register failed").Error();
        return false;
    }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, onIpEvent, this);
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] esp_event_handler_register failed").Error();
        return false;
    }
    // DHCP or static IP
    if (config.dhcp)
    {
        err = esp_netif_dhcpc_start(netifHandle);
        if (err != ESP_OK)
        {
            SigmaNetworkMgr::GetLog()->Append("[Error] esp_netif_dhcpc_start failed").Error();
        }
    }
    if (!config.dhcp || err != ESP_OK)
    { // static IP
        err = esp_netif_dhcpc_stop(netifHandle);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)
        {
            SigmaNetworkMgr::GetLog()->Append("[Error] esp_netif_dhcpc_stop failed").Error();
            return false;
        }
        esp_netif_ip_info_t ip_info = {};
        err = esp_netif_str_to_ip4(config.ip.toString().c_str(), &ip_info.ip);
        if (err != ESP_OK)
        {
            SigmaNetworkMgr::GetLog()->Append("[Error] IP Address is wrong:").Append(config.ip.toString()).Error();
            return false;
        }
        err = esp_netif_str_to_ip4(config.gateway.toString().c_str(), &ip_info.gw);
        if (err != ESP_OK)
        {
            SigmaNetworkMgr::GetLog()->Append("[Error] Gateway Address is wrong:").Append(config.gateway.toString()).Error();
            return false;
        }
        err = esp_netif_str_to_ip4(config.subnet.toString().c_str(), &ip_info.netmask);
        if (err != ESP_OK)
        {
            SigmaNetworkMgr::GetLog()->Append("[Error] Subnet Address is wrong:").Append(config.subnet.toString()).Error();
            return false;
        }
        err = esp_netif_set_ip_info(netifHandle, &ip_info);

        esp_netif_dns_info_t dns_info = {};
        err = esp_netif_str_to_ip4(config.dns.toString().c_str(), &dns_info.ip.u_addr.ip4);
        if (err != ESP_OK)
        {
            SigmaNetworkMgr::GetLog()->Append("[Error] DNS Address is wrong:").Append(config.dns.toString()).Error();
            return false;
        }
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        err = esp_netif_set_dns_info(netifHandle, ESP_NETIF_DNS_MAIN, &dns_info);
        if (err != ESP_OK)
        {
            SigmaNetworkMgr::GetLog()->Append("[Error] esp_netif_set_dns_info failed").Error();
            return false;
        }
    }
    return true;
}

bool SigmaEthernet::Connect()
{
    esp_err_t err = ESP_OK;
    err = esp_eth_start(ethHandle);
    if (err != ESP_OK)
    {
        SigmaNetworkMgr::GetLog()->Append("[Error] esp_eth_start failed").Error();
        return false;
    }
    SigmaNetworkMgr::GetLog()->Append("Ethernet started").Internal();

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
