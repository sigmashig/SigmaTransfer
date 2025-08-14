#include "SigmaSPI.h"

esp_err_t SigmaSPI::Initialize(SPIConfig &spiConfig, spi_device_handle_t *outDevice)
{
    esp_err_t err = ESP_OK;
    if (spiConfig.spiHost != SPI3_HOST && spiConfig.spiHost != SPI2_HOST && spiConfig.spiHost != SPI1_HOST)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!spiBusInited[static_cast<int>(spiConfig.spiHost)])
    {
        spi_bus_config_t buscfg = {};
        buscfg.miso_io_num = spiConfig.misoPin;
        buscfg.mosi_io_num = spiConfig.mosiPin;
        buscfg.sclk_io_num = spiConfig.sckPin;
        buscfg.quadwp_io_num = UNUSED_PIN;
        buscfg.quadhd_io_num = UNUSED_PIN;

        if (buscfg.miso_io_num == UNUSED_PIN || buscfg.mosi_io_num == UNUSED_PIN || buscfg.sclk_io_num == UNUSED_PIN ||
            buscfg.miso_io_num == 0 || buscfg.mosi_io_num == 0 || buscfg.sclk_io_num == 0)
        {
            if (spiConfig.spiHost == SPI3_HOST)
            {
                buscfg.miso_io_num = (buscfg.miso_io_num == 0) ? GPIO_NUM_19 : buscfg.miso_io_num;
                buscfg.mosi_io_num = (buscfg.mosi_io_num == 0) ? GPIO_NUM_23 : buscfg.mosi_io_num;
                buscfg.sclk_io_num = (buscfg.sclk_io_num == 0) ? GPIO_NUM_18 : buscfg.sclk_io_num;
            }
            else if (spiConfig.spiHost == SPI2_HOST)
            {
                buscfg.miso_io_num = (buscfg.miso_io_num == 0) ? GPIO_NUM_12 : buscfg.miso_io_num;
                buscfg.mosi_io_num = (buscfg.mosi_io_num == 0) ? GPIO_NUM_13 : buscfg.mosi_io_num;
                buscfg.sclk_io_num = (buscfg.sclk_io_num == 0) ? GPIO_NUM_14 : buscfg.sclk_io_num;
            }
            else if (spiConfig.spiHost == SPI1_HOST)
            {
                return ESP_ERR_INVALID_ARG;
            }
        }
        esp_err_t err = spi_bus_initialize(spiConfig.spiHost, &buscfg, defaultDmaChannel());
        if (err == ESP_OK)
        {
            spiBusInited[static_cast<int>(spiConfig.spiHost)] = true;
        }
        else
        {
            return err;
        }
    }
    /*
    Serial.printf("SPIConfig: host=%d, miso=%d, mosi=%d, sck=%d, cs=%d, clkMHz=%d\n", spiConfig.spiHost, spiConfig.misoPin, spiConfig.mosiPin, spiConfig.sckPin, spiConfig.csPin, spiConfig.spiClockMHz);
*/
    spi_device_interface_config_t devcfg = spiConfig.devcfg;
    if (devcfg.spics_io_num == UNUSED_PIN)
    {
        devcfg.spics_io_num = spiConfig.csPin;
    }
    if (devcfg.clock_speed_hz <= 0)
    {
        devcfg.clock_speed_hz = static_cast<int>(spiConfig.spiClockMHz) * 1000 * 1000;
    }
    if (devcfg.queue_size <= 0)
    {
        devcfg.queue_size = SPI_QUEUE_SIZE;
    }
    /*
    Serial.printf("SPI Device: cmdBits=%d, addrBits=%d, mode=%d, clkSpeed=%d, csPin=%d, queueSize=%d\n", devcfg.command_bits, devcfg.address_bits, devcfg.mode, devcfg.clock_speed_hz, devcfg.spics_io_num, devcfg.queue_size);
    */
    err = spi_bus_add_device(spiConfig.spiHost, &devcfg, outDevice);
    if (err != ESP_OK)
    {
        return err;
    }
    return ESP_OK;
}

spi_host_device_t SigmaSPI::SpiHostFromName(String name)
{
    if (name == "SPI3_HOST")
    {
        return SPI3_HOST;
    }
    else if (name == "SPI2_HOST")
    {
        return SPI2_HOST;
    }
    else if (name == "SPI1_HOST")
    {
        return SPI1_HOST;
    }
    return SPI3_HOST;
}
