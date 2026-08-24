#ifndef HTU21D_SENSOR_H
#define HTU21D_SENSOR_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  int16_t temperature_centi_c;
  uint16_t humidity_centi_rh;
} Htu21dMeasurement;

bool Htu21d_IsReady(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef Htu21d_ReadMeasurement(I2C_HandleTypeDef *hi2c,
                                         Htu21dMeasurement *measurement);

#endif /* HTU21D_SENSOR_H */
