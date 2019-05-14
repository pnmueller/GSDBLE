#include "app_error.h"
#include "app_util_platform.h"
#include "bmi160.h"
#include "boards.h"
#include "nrf_delay.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_spi.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include <string.h>

/**
hookup on 'bmi160 shuttle board' -> 'nrf dk52':
vdd -> vdd
vddio -> vdd
gnd -> gnd
miso -> P0.30
mosi -> P0.29
sck -> P0.27
cs -> P0.31
int1 -> P0.28
rest nc
*/
#define SPI_INSTANCE 0
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);
#define INT1_PIN 28
struct bmi160_dev bmi;

ret_code_t spi_init() {
  nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
  spi_config.ss_pin = 31;
  spi_config.miso_pin = 30;
  spi_config.mosi_pin = 29;
  spi_config.sck_pin = 27;
  spi_config.frequency = NRF_DRV_SPI_FREQ_125K;
  return nrf_drv_spi_init(&spi, &spi_config, NULL, NULL);
}

int8_t user_spi_read(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len) {
  //NRF_LOG_INFO("spi read %hu", len);

  uint8_t bigData[len + 1];

  ret_code_t err_code;
  err_code = nrf_drv_spi_transfer(&spi, &reg_addr, 1, bigData, len + 1);
  APP_ERROR_CHECK(err_code);

  memcpy(data, bigData + 1, len * sizeof(uint8_t));

  return BMI160_OK;
}

int8_t user_spi_write(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len) {
  // NRF_LOG_INFO("spi write %hu", len);

  uint16_t index = 0;
  while (index < len) {
    uint8_t p_tx_buffer[2];
    p_tx_buffer[0] = reg_addr;
    p_tx_buffer[1] = data[index];

    ret_code_t err_code;
    err_code = nrf_drv_spi_transfer(&spi, p_tx_buffer, 2, NULL, 0);
    APP_ERROR_CHECK(err_code);

    index++;
  }
  return BMI160_OK;
}

int8_t sensor_init() {
  memset(&bmi, 0, sizeof(struct bmi160_dev));

  bmi.id = 0;
  bmi.interface = BMI160_SPI_INTF;
  bmi.read = user_spi_read;
  bmi.write = user_spi_write;
  bmi.delay_ms = nrf_delay_ms;

  return bmi160_init(&bmi);
}

int8_t sensor_config() {
  bmi.accel_cfg.odr = BMI160_ACCEL_ODR_800HZ;
  bmi.accel_cfg.range = BMI160_ACCEL_RANGE_2G;
  bmi.accel_cfg.bw = BMI160_ACCEL_BW_NORMAL_AVG4;
  bmi.accel_cfg.power = BMI160_ACCEL_NORMAL_MODE;

  bmi.gyro_cfg.odr = BMI160_GYRO_ODR_800HZ;
  bmi.gyro_cfg.range = BMI160_GYRO_RANGE_2000_DPS;
  bmi.gyro_cfg.bw = BMI160_GYRO_BW_NORMAL_MODE;
  bmi.gyro_cfg.power = BMI160_GYRO_NORMAL_MODE;

  return bmi160_set_sens_conf(&bmi);
}

int8_t sensor_config_interrupt() {
  struct bmi160_int_pin_settg pin_config;
  memset(&pin_config, 0, sizeof(struct bmi160_int_pin_settg));
  pin_config.output_en = BMI160_ENABLE;
  pin_config.output_mode = BMI160_DISABLE;
  pin_config.output_type = BMI160_DISABLE;
  pin_config.edge_ctrl = BMI160_ENABLE;
  pin_config.input_en = BMI160_DISABLE;
  pin_config.latch_dur = BMI160_LATCH_DUR_NONE;

  struct bmi160_int_settg int_config;
  memset(&int_config, 0, sizeof(struct bmi160_int_settg));
  int_config.int_channel = BMI160_INT_CHANNEL_1;
  int_config.int_pin_settg = pin_config;
  int_config.int_type = BMI160_ACC_GYRO_FIFO_FULL_INT;
  int_config.fifo_full_int_en = BMI160_ENABLE;

  return bmi160_set_int_config(&int_config, &bmi);
}

void in_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
  NRF_LOG_INFO("Wakeup Event");
}

ret_code_t gpio_init(void) {
  ret_code_t err_code;

  err_code = nrf_drv_gpiote_init();
  APP_ERROR_CHECK(err_code);

  nrf_drv_gpiote_in_config_t in_config = NRFX_GPIOTE_RAW_CONFIG_IN_SENSE_HITOLO(true);
  in_config.pull = NRF_GPIO_PIN_NOPULL;

  err_code = nrf_drv_gpiote_in_init(INT1_PIN, &in_config, in_pin_handler);
  if (err_code == NRF_SUCCESS) {
    nrf_drv_gpiote_in_event_enable(INT1_PIN, true);
    return NRF_SUCCESS;
  } else
    return err_code;
}

/* An example to read the Gyro data in header mode along with sensor time (if available)
 * Configure the gyro sensor as prerequisite and follow the below example to read and
 * obtain the gyro data from FIFO */
int8_t fifo_gyro_header_time_data(struct bmi160_dev *dev) {
  int8_t rslt = 0;

  /* Declare memory to store the raw FIFO buffer information */
  uint8_t fifo_buff[300];

  /* Modify the FIFO buffer instance and link to the device instance */
  struct bmi160_fifo_frame fifo_frame;
  fifo_frame.data = fifo_buff;
  fifo_frame.length = 300;
  dev->fifo = &fifo_frame;
  uint16_t index = 0;

  /* Declare instances of the sensor data structure to store the parsed FIFO data */
  struct bmi160_sensor_data gyro_data[42]; // 300 bytes / ~7bytes per frame ~ 42 data frames
  uint8_t gyro_frames_req = 42;
  uint8_t gyro_index;

  /* Configure the sensor's FIFO settings */
  rslt = bmi160_set_fifo_config(BMI160_FIFO_GYRO, BMI160_ENABLE, dev);

  if (rslt == BMI160_OK) {
    dev->delay_ms(420);
    /* Read data from the sensor's FIFO and store it the FIFO buffer,"fifo_buff" */
    NRF_LOG_INFO("USER REQUESTED FIFO LENGTH : %d", dev->fifo->length);
    rslt = bmi160_get_fifo_data(dev);

    if (rslt == BMI160_OK) {
      NRF_LOG_INFO("AVAILABLE FIFO LENGTH : %d", dev->fifo->length);
      /* Print the raw FIFO data */
      for (index = 0; index < dev->fifo->length; index++) {
        NRF_LOG_INFO("FIFO DATA INDEX[%d] = %x", index, dev->fifo->data[index]);
        NRF_LOG_FLUSH();
      }
      /* Parse the FIFO data to extract gyro data from the FIFO buffer */
      NRF_LOG_INFO("REQUESTED GYRO DATA FRAMES : %d", gyro_frames_req);
      rslt = bmi160_extract_gyro(gyro_data, &gyro_frames_req, dev);

      if (rslt == BMI160_OK) {
        NRF_LOG_INFO("AVAILABLE GYRO DATA FRAMES : %d", gyro_frames_req);

        /* Print the parsed gyro data from the FIFO buffer */
        for (gyro_index = 0; gyro_index < gyro_frames_req; gyro_index++) {
          NRF_LOG_INFO("GYRO[%d] X-DATA : %d,  Y-DATA : %d,  Z-DATA : %d", gyro_index, gyro_data[gyro_index].x, gyro_data[gyro_index].y, gyro_data[gyro_index].z);
          NRF_LOG_FLUSH();
        }
        /* Print the special FIFO frame data like sensortime */
        NRF_LOG_INFO("SKIPPED FRAME COUNT : %d", dev->fifo->skipped_frame_count);
      } else {
        NRF_LOG_INFO("Gyro data extraction failed");
      }
    } else {
      NRF_LOG_INFO("Reading FIFO data failed");
    }
  } else {
    NRF_LOG_INFO("Setting FIFO configuration failed");
  }

  return rslt;
}

int main(void) {
  bsp_board_init(BSP_INIT_LEDS);

  APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  NRF_LOG_INFO("SPI example started.");

  APP_ERROR_CHECK(spi_init());
  APP_ERROR_CHECK_BOOL(BMI160_OK == sensor_init());
  APP_ERROR_CHECK_BOOL(BMI160_OK == sensor_config());
  APP_ERROR_CHECK_BOOL(BMI160_OK == bmi160_set_fifo_down(BMI160_GYRO_FIFO_DOWN_ONE, &bmi));
  APP_ERROR_CHECK_BOOL(BMI160_OK == sensor_config_interrupt());
  APP_ERROR_CHECK(gpio_init());

  NRF_LOG_INFO("Init finished");

  struct bmi160_fifo_frame fifo_frame;
  memset(&fifo_frame, 0, sizeof(struct bmi160_fifo_frame));
  bmi.fifo = &fifo_frame;

  uint8_t fifo_buff[1024];
  bmi.fifo->data = fifo_buff;
  bmi.fifo->length = 1024u;

  APP_ERROR_CHECK_BOOL(BMI160_OK == bmi160_set_fifo_config(BMI160_FIFO_GYRO, BMI160_ENABLE, &bmi));

  for (;;) {
    if (!NRF_LOG_PROCESS()) {
      __SEV();
      __WFE();
      __WFE();
      fifo_gyro_header_time_data(&bmi);
      for (;;) {
        if (!NRF_LOG_PROCESS()) {
          __SEV();
          __WFE();
          __WFE();
        }
      }
    }
  }
  /*
  // main
  while (true) {
    NRF_LOG_INFO("Read");
    memset(fifo_buff, 0, 1024 * sizeof(uint8_t));
    bmi.fifo->length = 1024u;
    NRF_LOG_INFO("USER REQUESTED FIFO LENGTH : %u", bmi.fifo->length);
    APP_ERROR_CHECK_BOOL(BMI160_OK == bmi160_get_fifo_data(&bmi));
    NRF_LOG_INFO("AVAILABLE FIFO LENGTH : %u", bmi.fifo->length);

    uint16_t index = 0;
    for (index = 0; index < bmi.fifo->length; index++) {
      NRF_LOG_INFO("FIFO DATA INDEX[%u] = %x", index, bmi.fifo->data[index]);
      NRF_LOG_FLUSH();
    }*/
  /*
    uint8_t gyro_frames_req = 80u;
    struct bmi160_sensor_data gyro_data[80];
    memset(gyro_data, 0, 80 * sizeof(struct bmi160_sensor_data));

    APP_ERROR_CHECK_BOOL(BMI160_OK == bmi160_extract_gyro(gyro_data, &gyro_frames_req, &bmi));
    NRF_LOG_INFO("GYRO DATA : %u", gyro_frames_req);
    NRF_LOG_INFO("SENSOR TIME DATA : %u", bmi.fifo->sensor_time);
    NRF_LOG_INFO("SKIPPED : %u.", bmi.fifo->skipped_frame_count);
    */
  /*
    uint8_t accel_frames_req = 100u;
    struct bmi160_sensor_data accel_data[100];

    APP_ERROR_CHECK_BOOL(BMI160_OK == bmi160_extract_accel(accel_data, &accel_frames_req, &bmi));
    NRF_LOG_INFO("ACCEL DATA : %u", accel_frames_req);
    NRF_LOG_INFO("SENSOR TIME DATA : %u", bmi.fifo->sensor_time);
    NRF_LOG_INFO("SKIPPED : %u.", bmi.fifo->skipped_frame_count);
*/
  /*

    NRF_LOG_FLUSH();
    __WFE();
    union bmi160_int_status status;
    APP_ERROR_CHECK_BOOL(BMI160_OK == bmi160_get_int_status(BMI160_INT_STATUS_1, &status, &bmi));
    if (status.bit.ffull == 0u) {
      NRF_LOG_INFO("Sleep");
      __WFE();
    }
    __SEV();
  }*/
}