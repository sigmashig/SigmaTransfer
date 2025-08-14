#pragma once

#include <Arduino.h>
#include "SigmaTransferDefs.h"

extern "C"
{
#include "driver/spi_master.h"
}

class SigmaSPI
{
public:
    static esp_err_t Initialize(SPIConfig &spiConfig, spi_device_handle_t *out_device);
    static spi_host_device_t SpiHostFromName(String name);

private:
    inline static bool spiBusInited[3] = {false, false, false};
    static inline int defaultDmaChannel()
    {
#ifdef SPI_DMA_CH_AUTO
        return SPI_DMA_CH_AUTO;
#else
        return SPI_DMA_CH1;
#endif
    }
    static constexpr int SPI_QUEUE_SIZE = 20;
    static constexpr int UNUSED_PIN = -1;
};
