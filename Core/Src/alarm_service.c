#include "alarm_service.h"

typedef struct
{
  uint16_t duration_ms;
  bool active;
} AlarmMelodyStep;

static const AlarmMelodyStep alarm_melody[] = {
  { 120U, true },
  { 80U, false },
  { 120U, true },
  { 80U, false },
  { 320U, true },
  { 700U, false },
};

static const AlarmMelodyStep alarm_soft_melody[] = {
  { 70U, true },
  { 430U, false },
  { 70U, true },
  { 950U, false },
};

static const AlarmMelodyStep alarm_enabled_feedback[] = {
  { 90U, true },
  { 70U, false },
  { 90U, true },
};

static const AlarmMelodyStep alarm_disabled_feedback[] = {
  { 260U, true },
};

static void BuzzerSet(AlarmService *service, bool active)
{
  if ((service == NULL) || (service->buzzer_port == NULL))
  {
    return;
  }

  HAL_GPIO_WritePin(service->buzzer_port, service->buzzer_pin,
                    active ? GPIO_PIN_RESET : GPIO_PIN_SET);
  service->buzzer_active = active;
}

static bool BuzzerSetIfChanged(AlarmService *service, bool active)
{
  if ((service == NULL) || (service->buzzer_active == active))
  {
    return false;
  }

  BuzzerSet(service, active);
  return true;
}

static bool IsAlarmTimeValid(uint8_t hours, uint8_t minutes)
{
  return (hours <= 23U) && (minutes <= 59U);
}

static void ResetMelody(AlarmService *service)
{
  if (service == NULL)
  {
    return;
  }

  service->melody_step = 0U;
  service->melody_step_elapsed_ms = 0U;
}

static void StopFeedback(AlarmService *service)
{
  if (service == NULL)
  {
    return;
  }

  service->feedback_active = false;
  service->feedback_enabled_tone = false;
  service->feedback_step = 0U;
  service->feedback_step_elapsed_ms = 0U;
}

static bool RunPattern(AlarmService *service, const AlarmMelodyStep *pattern,
                       uint8_t pattern_length, uint8_t *step_index,
                       uint16_t *step_elapsed_ms, uint16_t elapsed_ms,
                       bool repeat)
{
  bool changed = false;

  if ((service == NULL) || (pattern == NULL) || (step_index == NULL) ||
      (step_elapsed_ms == NULL) || (pattern_length == 0U))
  {
    return false;
  }

  while (elapsed_ms > 0U)
  {
    const AlarmMelodyStep *step = &pattern[*step_index];
    uint16_t remaining_ms = step->duration_ms - *step_elapsed_ms;

    changed = BuzzerSetIfChanged(service, step->active) || changed;

    if (elapsed_ms < remaining_ms)
    {
      *step_elapsed_ms = (uint16_t) (*step_elapsed_ms + elapsed_ms);
      break;
    }

    elapsed_ms = (uint16_t) (elapsed_ms - remaining_ms);
    *step_elapsed_ms = 0U;
    (*step_index)++;

    if (*step_index >= pattern_length)
    {
      if (repeat)
      {
        *step_index = 0U;
      }
      else
      {
        changed = BuzzerSetIfChanged(service, false) || changed;
        break;
      }
    }
  }

  return changed;
}

void AlarmService_Init(AlarmService *service, I2C_HandleTypeDef *rtc_i2c,
                       GPIO_TypeDef *buzzer_port, uint16_t buzzer_pin)
{
  if (service == NULL)
  {
    return;
  }

  service->rtc_i2c = rtc_i2c;
  service->buzzer_port = buzzer_port;
  service->buzzer_pin = buzzer_pin;
  service->hours = 7U;
  service->minutes = 30U;
  service->enabled = false;
  service->ringing = false;
  service->soft_ringing = false;
  service->buzzer_active = false;
  service->melody_step = 0U;
  service->melody_step_elapsed_ms = 0U;
  service->feedback_active = false;
  service->feedback_enabled_tone = false;
  service->feedback_step = 0U;
  service->feedback_step_elapsed_ms = 0U;
  service->rtc_interrupt_pending = false;

  BuzzerSet(service, false);
}

HAL_StatusTypeDef AlarmService_SetAlarm(AlarmService *service, uint8_t hours,
                                        uint8_t minutes, bool enabled)
{
  HAL_StatusTypeDef status;

  if ((service == NULL) || (service->rtc_i2c == NULL) ||
      !IsAlarmTimeValid(hours, minutes))
  {
    return HAL_ERROR;
  }

  service->hours = hours;
  service->minutes = minutes;
  service->enabled = enabled;
  service->ringing = false;
  service->soft_ringing = false;
  ResetMelody(service);
  StopFeedback(service);
  BuzzerSet(service, false);

  status = RtcDs3231_ClearAlarm1Flag(service->rtc_i2c);
  if (status != HAL_OK)
  {
    return status;
  }

  if (!enabled)
  {
    return RtcDs3231_EnableAlarm1Interrupt(service->rtc_i2c, false);
  }

  status = RtcDs3231_SetAlarm1Daily(service->rtc_i2c, hours, minutes, 0U);
  if (status != HAL_OK)
  {
    return status;
  }

  return RtcDs3231_EnableAlarm1Interrupt(service->rtc_i2c, true);
}

void AlarmService_HandleRtcInterruptFromIsr(AlarmService *service)
{
  if (service != NULL)
  {
    service->rtc_interrupt_pending = true;
  }
}

void AlarmService_Process(AlarmService *service)
{
  bool alarm_flag = false;

  if ((service == NULL) || (service->rtc_i2c == NULL) ||
      !service->rtc_interrupt_pending)
  {
    return;
  }

  service->rtc_interrupt_pending = false;

  if (RtcDs3231_IsAlarm1FlagSet(service->rtc_i2c, &alarm_flag) != HAL_OK)
  {
    return;
  }

  if (alarm_flag && service->enabled)
  {
    service->ringing = true;
    service->soft_ringing = false;
    ResetMelody(service);
    StopFeedback(service);
    BuzzerSet(service, true);
  }
  else if (alarm_flag)
  {
    (void) RtcDs3231_ClearAlarm1Flag(service->rtc_i2c);
  }
}

bool AlarmService_OnTick(AlarmService *service, uint16_t elapsed_ms)
{
  bool changed = false;

  if (service == NULL)
  {
    return false;
  }

  if (!service->ringing)
  {
    ResetMelody(service);
    if (service->feedback_active)
    {
      const AlarmMelodyStep *feedback =
          service->feedback_enabled_tone ? alarm_enabled_feedback :
                                           alarm_disabled_feedback;
      uint8_t feedback_length =
          service->feedback_enabled_tone ?
              (uint8_t) (sizeof(alarm_enabled_feedback) /
                         sizeof(alarm_enabled_feedback[0])) :
              (uint8_t) (sizeof(alarm_disabled_feedback) /
                         sizeof(alarm_disabled_feedback[0]));

      changed = RunPattern(service, feedback, feedback_length,
                           &service->feedback_step,
                           &service->feedback_step_elapsed_ms, elapsed_ms,
                           false);

      if (service->feedback_step >= feedback_length)
      {
        StopFeedback(service);
      }

      return changed;
    }

    return BuzzerSetIfChanged(service, false);
  }

  StopFeedback(service);
  if (service->soft_ringing)
  {
    return RunPattern(service, alarm_soft_melody,
                    (uint8_t) (sizeof(alarm_soft_melody) /
                               sizeof(alarm_soft_melody[0])),
                    &service->melody_step, &service->melody_step_elapsed_ms,
                    elapsed_ms, true);
  }

  return RunPattern(service, alarm_melody,
                    (uint8_t) (sizeof(alarm_melody) / sizeof(alarm_melody[0])),
                    &service->melody_step, &service->melody_step_elapsed_ms,
                    elapsed_ms, true);
}

void AlarmService_SetSoftRinging(AlarmService *service, bool soft_enabled)
{
  if ((service == NULL) || (service->soft_ringing == soft_enabled))
  {
    return;
  }

  service->soft_ringing = soft_enabled;
  ResetMelody(service);
}

void AlarmService_PlayToggleFeedback(AlarmService *service, bool enabled)
{
  if ((service == NULL) || service->ringing)
  {
    return;
  }

  service->feedback_active = true;
  service->feedback_enabled_tone = enabled;
  service->feedback_step = 0U;
  service->feedback_step_elapsed_ms = 0U;
}

HAL_StatusTypeDef AlarmService_Dismiss(AlarmService *service)
{
  HAL_StatusTypeDef status = HAL_OK;

  if (service == NULL)
  {
    return HAL_ERROR;
  }

  service->ringing = false;
  service->soft_ringing = false;
  ResetMelody(service);
  StopFeedback(service);
  BuzzerSet(service, false);

  if (service->rtc_i2c != NULL)
  {
    status = RtcDs3231_ClearAlarm1Flag(service->rtc_i2c);
  }

  return status;
}

bool AlarmService_IsRinging(const AlarmService *service)
{
  return (service != NULL) && service->ringing;
}
