#include "lsm_protocol.h"
#include "nrf_drv_spi.h"
#include "nrf_log.h"

/**
implementation of lsm_protocol.h. uses spi as protocol

hookup on 'steval-mk178v2' -> 'nrf dk52':
miso -> P0.4
mosi -> P0.28
sck -> P0.29
cs -> P0.30
*/
#define SPI_INSTANCE 0
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);

void lsm_protocol_init() {
  nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
  spi_config.ss_pin = 30;
  spi_config.miso_pin = 4;
  spi_config.mosi_pin = 28;
  spi_config.sck_pin = 29;
  spi_config.frequency = NRF_DRV_SPI_FREQ_8M;
  APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, NULL, NULL));
}

int32_t lsm_protocol_read(void *handle, uint8_t reg_addr, uint8_t *data, uint16_t len) {
  //NRF_LOG_INFO("spi read %u on %02x", len, reg_addr);
  if (len > 0xFEu)
    NRF_LOG_INFO("SPI-READ: YOU ARE READING TOO MUCH DATA! %u", len);

  uint8_t bigData[len + 1];

  ret_code_t err_code;
  uint8_t address = reg_addr | 0x80;
  err_code = nrf_drv_spi_transfer(&spi, &address, 1, bigData, len + 1);
  APP_ERROR_CHECK(err_code);

  memcpy(data, bigData + 1, len * sizeof(uint8_t));

  return 0;
}
int32_t lsm_protocol_write(void *handle, uint8_t reg_addr, uint8_t *data, uint16_t len) {
  //NRF_LOG_INFO("spi write %u on %02x", len, reg_addr);
  if (len > 0xFEu)
    NRF_LOG_INFO("SPI-WRITE: YOU ARE WRITING TOO MUCH DATA! %u", len);

  uint8_t bigData[len + 1];
  bigData[0] = reg_addr & 0x7F;
  memcpy(bigData + 1, data, len);
  ret_code_t err_code;
  err_code = nrf_drv_spi_transfer(&spi, bigData, len + 1, NULL, 0);
  APP_ERROR_CHECK(err_code);
  return 0;
}