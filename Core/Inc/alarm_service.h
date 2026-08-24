#ifndef ALARM_SERVICE_H
#define ALARM_SERVICE_H

#include "rtc_ds3231.h"
#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  I2C_HandleTypeDef *rtc_i2c;
  GPIO_TypeDef *buzzer_port;
  uint16_t buzzer_pin;
  uint8_t hours;
  uint8_t minutes;
  bool enabled;
  bool ringing;
  bool soft_ringing;
  bool buzzer_active;
  uint8_t melody_step;
  uint16_t melody_step_elapsed_ms;
  bool feedback_active;
  bool feedback_enabled_tone;
  uint8_t feedback_step;
  uint16_t feedback_step_elapsed_ms;
  volatile bool rtc_interrupt_pending;
} AlarmService;

void AlarmService_Init(AlarmService *service, I2C_HandleTypeDef *rtc_i2c,
                       GPIO_TypeDef *buzzer_port, uint16_t buzzer_pin);
HAL_StatusTypeDef AlarmService_SetAlarm(AlarmService *service, uint8_t hours,
                                        uint8_t minutes, bool enabled);
void AlarmService_HandleRtcInterruptFromIsr(AlarmService *service);
void AlarmService_Process(AlarmService *service);
bool AlarmService_OnTick(AlarmService *service, uint16_t elapsed_ms);
void AlarmService_SetSoftRinging(AlarmService *service, bool soft_enabled);
void AlarmService_PlayToggleFeedback(AlarmService *service, bool enabled);
HAL_StatusTypeDef AlarmService_Dismiss(AlarmService *service);
bool AlarmService_IsRinging(const AlarmService *service);

#endif
