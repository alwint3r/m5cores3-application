#include "ispi/ispi.h"

#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <limits.h>
#include <soc/soc_caps.h>
#include <stdlib.h>

struct ispi_master_bus_t {
  spi_host_device_t esp_host;
};

struct ispi_device_t {
  spi_device_handle_t esp_dev;
  bool half_duplex;
};

static int32_t esp_err_to_ispi(esp_err_t err) {
  switch (err) {
    case ESP_OK:
      return ISPI_ERR_NONE;
    case ESP_ERR_NO_MEM:
      return ISPI_ERR_NO_MEM;
    case ESP_ERR_INVALID_ARG:
      return ISPI_ERR_INVALID_ARG;
    case ESP_ERR_INVALID_STATE:
      return ISPI_ERR_INVALID_STATE;
    case ESP_ERR_NOT_FOUND:
      return ISPI_ERR_NOT_FOUND;
    case ESP_ERR_NOT_SUPPORTED:
      return ISPI_ERR_NOT_SUPPORTED;
    case ESP_ERR_TIMEOUT:
      return ISPI_ERR_TIMEOUT;
    default:
      return ISPI_ERR_IO;
  }
}

static int32_t ispi_host_to_esp(ispi_host_t host, spi_host_device_t *ret_host) {
  if (ret_host == NULL) {
    return ISPI_ERR_INVALID_ARG;
  }

  switch (host) {
    case ISPI_HOST_SPI2:
      *ret_host = SPI2_HOST;
      return ISPI_ERR_NONE;
    case ISPI_HOST_SPI3:
#if SOC_SPI_PERIPH_NUM > 2
      *ret_host = SPI3_HOST;
      return ISPI_ERR_NONE;
#else
      return ISPI_ERR_NOT_SUPPORTED;
#endif
    default:
      return ISPI_ERR_INVALID_ARG;
  }
}

static int32_t ispi_dma_to_esp(ispi_dma_channel_t dma, spi_dma_chan_t *ret_dma) {
  if (ret_dma == NULL) {
    return ISPI_ERR_INVALID_ARG;
  }

  switch (dma) {
    case ISPI_DMA_DISABLED:
      *ret_dma = SPI_DMA_DISABLED;
      return ISPI_ERR_NONE;
    case ISPI_DMA_AUTO:
      *ret_dma = SPI_DMA_CH_AUTO;
      return ISPI_ERR_NONE;
    default:
      return ISPI_ERR_INVALID_ARG;
  }
}

const char *ispi_err_to_name(int32_t err) {
  switch (err) {
    case ISPI_ERR_NONE:
      return "ISPI_ERR_NONE";
    case ISPI_ERR_FAIL:
      return "ISPI_ERR_FAIL";
    case ISPI_ERR_NO_MEM:
      return "ISPI_ERR_NO_MEM";
    case ISPI_ERR_INVALID_ARG:
      return "ISPI_ERR_INVALID_ARG";
    case ISPI_ERR_INVALID_STATE:
      return "ISPI_ERR_INVALID_STATE";
    case ISPI_ERR_NOT_FOUND:
      return "ISPI_ERR_NOT_FOUND";
    case ISPI_ERR_NOT_SUPPORTED:
      return "ISPI_ERR_NOT_SUPPORTED";
    case ISPI_ERR_TIMEOUT:
      return "ISPI_ERR_TIMEOUT";
    case ISPI_ERR_IO:
      return "ISPI_ERR_IO";
    default:
      return "ISPI_ERR_UNKNOWN";
  }
}

void ispi_get_default_master_bus_config(ispi_master_bus_config_t *config) {
  if (config == NULL) {
    return;
  }

  config->host = ISPI_HOST_SPI2;
  config->mosi_io_num = -1;
  config->miso_io_num = -1;
  config->sclk_io_num = -1;
  config->max_transfer_sz = 0;
  config->dma_channel = ISPI_DMA_AUTO;
}

void ispi_get_default_device_config(ispi_device_config_t *config) {
  if (config == NULL) {
    return;
  }

  config->cs_io_num = -1;
  config->clock_speed_hz = 1000000U;
  config->mode = 0;
  config->input_delay_ns = 0;
  config->queue_size = 1;
  config->lsb_first = false;
  config->half_duplex = false;
  config->use_3wire = false;
}

void ispi_get_default_transaction(ispi_transaction_t *transaction) {
  if (transaction == NULL) {
    return;
  }

  transaction->tx_buffer = NULL;
  transaction->tx_size = 0;
  transaction->rx_buffer = NULL;
  transaction->rx_size = 0;
}

int32_t ispi_new_master_bus(const ispi_master_bus_config_t *config,
                            ispi_master_bus_handle_t *ret_bus) {
  if (config == NULL || ret_bus == NULL) {
    return ISPI_ERR_INVALID_ARG;
  }
  if (config->sclk_io_num < 0) {
    return ISPI_ERR_INVALID_ARG;
  }
  if (config->mosi_io_num < 0 && config->miso_io_num < 0) {
    return ISPI_ERR_INVALID_ARG;
  }
  if (config->max_transfer_sz < 0) {
    return ISPI_ERR_INVALID_ARG;
  }

  spi_host_device_t esp_host = SPI2_HOST;
  int32_t rc = ispi_host_to_esp(config->host, &esp_host);
  if (rc != ISPI_ERR_NONE) {
    return rc;
  }

  spi_dma_chan_t esp_dma = SPI_DMA_CH_AUTO;
  rc = ispi_dma_to_esp(config->dma_channel, &esp_dma);
  if (rc != ISPI_ERR_NONE) {
    return rc;
  }

  spi_bus_config_t esp_config = {
      .mosi_io_num = config->mosi_io_num,
      .miso_io_num = config->miso_io_num,
      .sclk_io_num = config->sclk_io_num,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .data4_io_num = -1,
      .data5_io_num = -1,
      .data6_io_num = -1,
      .data7_io_num = -1,
      .data_io_default_level = false,
      .max_transfer_sz = config->max_transfer_sz,
      .flags = 0U,
      .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
      .intr_flags = 0,
  };

  rc = esp_err_to_ispi(spi_bus_initialize(esp_host, &esp_config, esp_dma));
  if (rc != ISPI_ERR_NONE) {
    return rc;
  }

  struct ispi_master_bus_t *bus = calloc(1, sizeof(*bus));
  if (bus == NULL) {
    (void)spi_bus_free(esp_host);
    return ISPI_ERR_NO_MEM;
  }

  bus->esp_host = esp_host;
  *ret_bus = bus;
  return ISPI_ERR_NONE;
}

int32_t ispi_del_master_bus(ispi_master_bus_handle_t bus) {
  if (bus == NULL) {
    return ISPI_ERR_INVALID_ARG;
  }

  int32_t rc = esp_err_to_ispi(spi_bus_free(bus->esp_host));
  if (rc != ISPI_ERR_NONE) {
    return rc;
  }

  free(bus);
  return ISPI_ERR_NONE;
}

int32_t ispi_new_device(ispi_master_bus_handle_t bus,
                        const ispi_device_config_t *config,
                        ispi_device_handle_t *ret_device) {
  if (bus == NULL || config == NULL || ret_device == NULL) {
    return ISPI_ERR_INVALID_ARG;
  }
  if (config->clock_speed_hz == 0U || config->clock_speed_hz > (uint32_t)INT_MAX) {
    return ISPI_ERR_INVALID_ARG;
  }
  if (config->mode > 3U || config->queue_size == 0U) {
    return ISPI_ERR_INVALID_ARG;
  }
  if (config->cs_io_num < -1 || config->input_delay_ns < 0) {
    return ISPI_ERR_INVALID_ARG;
  }

  uint32_t flags = 0U;
  if (config->lsb_first) {
    flags |= SPI_DEVICE_BIT_LSBFIRST;
  }
  if (config->half_duplex) {
    flags |= SPI_DEVICE_HALFDUPLEX;
  }
  if (config->use_3wire) {
    flags |= SPI_DEVICE_3WIRE;
  }

  spi_device_interface_config_t esp_config = {
      .command_bits = 0,
      .address_bits = 0,
      .dummy_bits = 0,
      .mode = config->mode,
      .clock_source = SPI_CLK_SRC_DEFAULT,
      .duty_cycle_pos = 0,
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .clock_speed_hz = (int)config->clock_speed_hz,
      .input_delay_ns = config->input_delay_ns,
      .sample_point = SPI_SAMPLING_POINT_PHASE_0,
      .spics_io_num = config->cs_io_num,
      .flags = flags,
      .queue_size = (int)config->queue_size,
      .pre_cb = NULL,
      .post_cb = NULL,
  };

  spi_device_handle_t esp_dev = NULL;
  int32_t rc = esp_err_to_ispi(spi_bus_add_device(bus->esp_host, &esp_config, &esp_dev));
  if (rc != ISPI_ERR_NONE) {
    return rc;
  }

  struct ispi_device_t *device = calloc(1, sizeof(*device));
  if (device == NULL) {
    (void)spi_bus_remove_device(esp_dev);
    return ISPI_ERR_NO_MEM;
  }

  device->esp_dev = esp_dev;
  device->half_duplex = config->half_duplex;
  *ret_device = device;
  return ISPI_ERR_NONE;
}

int32_t ispi_del_device(ispi_device_handle_t device) {
  if (device == NULL) {
    return ISPI_ERR_INVALID_ARG;
  }

  int32_t rc = esp_err_to_ispi(spi_bus_remove_device(device->esp_dev));
  if (rc != ISPI_ERR_NONE) {
    return rc;
  }

  free(device);
  return ISPI_ERR_NONE;
}

int32_t ispi_device_transfer(ispi_device_handle_t device,
                             const ispi_transaction_t *transaction) {
  if (device == NULL || transaction == NULL) {
    return ISPI_ERR_INVALID_ARG;
  }
  if ((transaction->tx_size == 0U) != (transaction->tx_buffer == NULL)) {
    return ISPI_ERR_INVALID_ARG;
  }
  if ((transaction->rx_size == 0U) != (transaction->rx_buffer == NULL)) {
    return ISPI_ERR_INVALID_ARG;
  }
  if (transaction->tx_size == 0U && transaction->rx_size == 0U) {
    return ISPI_ERR_INVALID_ARG;
  }

  spi_transaction_t esp_transaction = {
      .flags = 0U,
      .cmd = 0U,
      .addr = 0U,
      .length = 0U,
      .rxlength = 0U,
      .override_freq_hz = 0U,
      .user = NULL,
      .tx_buffer = transaction->tx_buffer,
      .rx_buffer = transaction->rx_buffer,
  };

  if (transaction->tx_size != 0U && transaction->rx_size != 0U) {
    if (device->half_duplex) {
      esp_transaction.length = transaction->tx_size * 8U;
      esp_transaction.rxlength = transaction->rx_size * 8U;
    } else {
      if (transaction->tx_size != transaction->rx_size) {
        return ISPI_ERR_INVALID_ARG;
      }
      esp_transaction.length = transaction->tx_size * 8U;
      esp_transaction.rxlength = transaction->rx_size * 8U;
    }
  } else if (transaction->tx_size != 0U) {
    esp_transaction.length = transaction->tx_size * 8U;
    esp_transaction.rxlength = 0U;
  } else {
    esp_transaction.length = transaction->rx_size * 8U;
    esp_transaction.rxlength = transaction->rx_size * 8U;
  }

  return esp_err_to_ispi(spi_device_transmit(device->esp_dev, &esp_transaction));
}
