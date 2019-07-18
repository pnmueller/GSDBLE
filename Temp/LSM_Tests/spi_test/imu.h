#ifndef IMU_H_
#define IMU_H_

#include <stdint.h>

/**
definitions to control an imu from an uc
*/
typedef enum {
  IMU_ODR_25Hz = 0,
  IMU_ODR_50Hz = 1,
  IMU_ODR_100Hz = 2,
  IMU_ODR_200Hz = 3,
  IMU_ODR_400Hz = 4,
  IMU_ODR_800Hz = 5,
  IMU_ODR_1600Hz = 6,
} imu_speed_t;

typedef struct {
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
  uint32_t time;
} imu_data;

/** 
init imu and all protocols. 

parameter is the function pointer, where the data is going to be delivered.
its return boolean is true, if the data could be sent, otherwise false
*/
void imu_init(bool (*send_data)(imu_data));

/** function that is called in the main loop */
void imu_loop();

/** set speed of data */
void imu_set_speed(imu_speed_t speed);

/** get speed of data */
imu_speed_t imu_get_speed();

/** stop sending data */
void imu_stop();

/** continue sending data */
void imu_restart();

#endif