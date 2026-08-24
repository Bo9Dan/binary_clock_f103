#ifndef RTC_DS3231_H
#define RTC_DS3231_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t weekday;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} RtcDs3231_DateTime;

typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} RtcDs3231_Time;

bool RtcDs3231_IsReady(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef RtcDs3231_ReadDateTime(I2C_HandleTypeDef *hi2c,
                                         RtcDs3231_DateTime *date_time);
HAL_StatusTypeDef RtcDs3231_WriteDateTime(I2C_HandleTypeDef *hi2c,
                                          const RtcDs3231_DateTime *date_time);
HAL_StatusTypeDef RtcDs3231_IsOscillatorStopFlagSet(I2C_HandleTypeDef *hi2c,
                                                    bool *is_set);
HAL_StatusTypeDef RtcDs3231_ClearOscillatorStopFlag(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef RtcDs3231_ReadTime(I2C_HandleTypeDef *hi2c,
                                     RtcDs3231_Time *time);
HAL_StatusTypeDef RtcDs3231_WriteTime(I2C_HandleTypeDef *hi2c,
                                      const RtcDs3231_Time *time);
HAL_StatusTypeDef RtcDs3231_SetAlarm1Daily(I2C_HandleTypeDef *hi2c,
                                           uint8_t hours, uint8_t minutes,
                                           uint8_t seconds);
HAL_StatusTypeDef RtcDs3231_EnableAlarm1Interrupt(I2C_HandleTypeDef *hi2c,
                                                  bool enabled);
HAL_StatusTypeDef RtcDs3231_ClearAlarm1Flag(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef RtcDs3231_IsAlarm1FlagSet(I2C_HandleTypeDef *hi2c,
                                            bool *is_set);

#endif
