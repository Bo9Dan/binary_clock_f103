#ifndef APDS9960_SENSOR_H
#define APDS9960_SENSOR_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint16_t clear;
  uint16_t red;
  uint16_t green;
  uint16_t blue;
} Apds9960AmbientLight;

typedef enum
{
  APDS9960_GESTURE_NONE = 0,
  APDS9960_GESTURE_LEFT,
  APDS9960_GESTURE_RIGHT,
  APDS9960_GESTURE_UP,
  APDS9960_GESTURE_DOWN,
} Apds9960Gesture;

bool Apds9960_IsReady(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef Apds9960_ReadId(I2C_HandleTypeDef *hi2c, uint8_t *id);
HAL_StatusTypeDef Apds9960_InitAmbientLight(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef Apds9960_ReadAmbientLight(I2C_HandleTypeDef *hi2c,
                                            Apds9960AmbientLight *light);
HAL_StatusTypeDef Apds9960_ReadProximity(I2C_HandleTypeDef *hi2c,
                                         uint8_t *proximity);
HAL_StatusTypeDef Apds9960_InitGesture(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef Apds9960_ReadGesture(I2C_HandleTypeDef *hi2c,
                                       Apds9960Gesture *gesture);

#endif /* APDS9960_SENSOR_H */
