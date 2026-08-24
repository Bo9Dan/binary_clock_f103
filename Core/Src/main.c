/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "alarm_service.h"
#include "app_scheduler.h"
#include "apds9960_sensor.h"
#include "htu21d_sensor.h"
#include "rtc_ds3231.h"
#include <stdio.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  const char *name;
  GPIO_TypeDef *port;
  uint16_t pin;
  GPIO_PinState stable_level;
  GPIO_PinState last_raw_level;
  uint32_t last_raw_change_ms;
  uint32_t pressed_since_ms;
} ButtonTestState;

typedef enum
{
  APP_DISPLAY_MODE_TIME = 0,
  APP_DISPLAY_MODE_DATE,
  APP_DISPLAY_MODE_ENV,
  APP_DISPLAY_MODE_ALARM,
} AppDisplayMode;

typedef enum
{
  APP_EDIT_FIELD_NONE = 0,
  APP_EDIT_FIELD_TIME_HOURS,
  APP_EDIT_FIELD_TIME_MINUTES,
  APP_EDIT_FIELD_DATE_MONTH,
  APP_EDIT_FIELD_DATE_DAY,
  APP_EDIT_FIELD_ALARM_HOURS,
  APP_EDIT_FIELD_ALARM_MINUTES,
} AppEditField;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SHIFT_REGISTER_BYTE_COUNT 3U
#define SHIFT_REGISTER_TIMEOUT_MS 10U
#define BRINGUP_STEP_DELAY_MS 500U
#define DISPLAY_STATUS_EDITING (1U << 1U)
#define DISPLAY_STATUS_TIME (1U << 2U)
#define DISPLAY_STATUS_DATE (1U << 3U)
#define DISPLAY_STATUS_ENV (1U << 4U)
#define DISPLAY_STATUS_ALARM (1U << 5U)
#define BRIGHTNESS_MIN_PERCENT 5U
#define BRIGHTNESS_MAX_PERCENT 100U
#define BRIGHTNESS_LOW_LIGHT_CLEAR 20U
#define BRIGHTNESS_HIGH_LIGHT_CLEAR 800U
#define BRIGHTNESS_SMOOTH_STEP_PERCENT 5U
#define GESTURE_ALARM_SEQUENCE_TIMEOUT_MS 1500U
#define PROXIMITY_SOFT_ALARM_ON 80U
#define PROXIMITY_SOFT_ALARM_OFF 35U
#define RTC_WRITE_EXAMPLE_ON_BOOT 0
#define BUTTON_COUNT 4U
#define BUTTON_MODE_INDEX 0U
#define BUTTON_PLUS_INDEX 1U
#define BUTTON_MINUS_INDEX 2U
#define BUTTON_OK_INDEX 3U
#define BUTTON_DEBOUNCE_MS 30U
#define BUTTON_LONG_PRESS_MS 800U
#define APP_DISPLAY_MODE_COUNT 4U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static ButtonTestState button_test_states[BUTTON_COUNT] = {
  { "BTN_MODE", BTN_MODE_GPIO_Port, BTN_MODE_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0U, 0U },
  { "BTN_PLUS", BTN_PLUS_GPIO_Port, BTN_PLUS_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0U, 0U },
  { "BTN_MINUS", BTN_MINUS_GPIO_Port, BTN_MINUS_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0U, 0U },
  { "BTN_OK", BTN_OK_GPIO_Port, BTN_OK_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0U, 0U },
};
static AppDisplayMode app_display_mode = APP_DISPLAY_MODE_TIME;
static AppEditField app_edit_field = APP_EDIT_FIELD_NONE;
static bool app_edit_blink_on = true;
static uint8_t app_status_feedback_ticks = 0U;
static bool app_status_feedback_on = false;
static uint8_t app_edit_hours = 0U;
static uint8_t app_edit_minutes = 0U;
static uint8_t app_edit_month = 1U;
static uint8_t app_edit_day = 1U;
static RtcDs3231_DateTime latest_rtc_time;
static bool latest_rtc_time_valid = false;
static Htu21dMeasurement latest_env_measurement;
static bool latest_env_measurement_valid = false;
static AlarmService alarm_service;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void Debug_WriteLine(const char *text);
static void Debug_WriteDateTime(const RtcDs3231_DateTime *date_time);
static void Debug_WriteMeasurement(const Htu21dMeasurement *measurement);
static void Debug_WriteAmbientLight(const Apds9960AmbientLight *light);
static void ButtonTest_Init(uint32_t now_ms);
static void ButtonTest_Update(uint32_t now_ms);
static uint8_t Brightness_CalculateTargetPercent(uint16_t clear_value);
static uint8_t Brightness_StepToward(uint8_t current_percent,
                                     uint8_t target_percent);
static void Brightness_SetPercent(uint8_t brightness_percent);
static uint8_t Display_EncodeBinary7(uint8_t value);
static uint8_t Display_BuildStatus(uint8_t mode_status);
static void Display_WriteRows(uint8_t top, uint8_t bottom, uint8_t status,
                              bool show_top, bool show_bottom);
static void Display_WriteTime(uint8_t hours, uint8_t minutes);
static void Display_WriteDate(uint8_t month, uint8_t day);
static void Display_WriteEnvironment(const Htu21dMeasurement *measurement);
static void Display_WriteAlarm(void);
static void Display_WriteEdit(void);
static void Display_WriteCurrentMode(const RtcDs3231_DateTime *date_time);
static const char *App_DisplayModeName(AppDisplayMode mode);
static const char *App_GestureName(Apds9960Gesture gesture);
static bool App_IsEditing(void);
static void App_SetDisplayMode(AppDisplayMode mode);
static void App_NextDisplayMode(void);
static void App_PreviousDisplayMode(void);
static uint8_t App_DaysInMonth(uint16_t year, uint8_t month);
static uint8_t App_WrapValue(uint8_t value, int8_t delta, uint8_t min_value,
                             uint8_t max_value);
static void App_StartSetupForCurrentMode(void);
static void App_CancelEdit(void);
static void App_AdjustEditValue(int8_t delta);
static void App_AdvanceOrSaveEdit(void);
static void App_HandleButtonShortPress(uint32_t button_index);
static void App_HandleOkButtonShortPress(void);
static bool App_IsAlarmRinging(void);
static void App_SnoozeAlarmOneMinute(void);
static void App_DismissAlarm(void);
static void App_HandleGesture(Apds9960Gesture gesture, uint32_t now_ms);
static void App_ReadRtcAndRefresh(void);
static void App_ReadEnvironmentAndRefresh(void);
static void App_ReadAmbientLightAndUpdate(uint8_t *current_brightness_percent);
static void ShiftRegister_Write24(uint32_t value);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Debug_WriteLine(const char *text)
{
  if (text == NULL)
  {
    return;
  }

  (void) HAL_UART_Transmit(&huart1, (uint8_t *) text, strlen(text),
                           HAL_MAX_DELAY);
  (void) HAL_UART_Transmit(&huart1, (uint8_t *) "\r\n", 2U, HAL_MAX_DELAY);
}

static void Debug_WriteDateTime(const RtcDs3231_DateTime *date_time)
{
  char line[48];

  if (date_time == NULL)
  {
    return;
  }

  (void) snprintf(line, sizeof(line), "RTC %04u-%02u-%02u %02u:%02u:%02u",
                  date_time->year, date_time->month, date_time->day,
                  date_time->hours, date_time->minutes, date_time->seconds);
  Debug_WriteLine(line);
}

static void Debug_WriteMeasurement(const Htu21dMeasurement *measurement)
{
  char line[64];
  int16_t temperature;
  uint16_t humidity;
  char temperature_sign = '+';

  if (measurement == NULL)
  {
    return;
  }

  temperature = measurement->temperature_centi_c;
  humidity = measurement->humidity_centi_rh;

  if (temperature < 0)
  {
    temperature_sign = '-';
    temperature = (int16_t) -temperature;
  }

  (void) snprintf(line, sizeof(line), "ENV %c%d.%02d C %u.%02u %%RH",
                  temperature_sign, temperature / 100, temperature % 100,
                  humidity / 100U, humidity % 100U);
  Debug_WriteLine(line);
}

static void Debug_WriteAmbientLight(const Apds9960AmbientLight *light)
{
  char line[72];

  if (light == NULL)
  {
    return;
  }

  (void) snprintf(line, sizeof(line), "LIGHT C=%u R=%u G=%u B=%u",
                  light->clear, light->red, light->green, light->blue);
  Debug_WriteLine(line);
}

static void ButtonTest_Init(uint32_t now_ms)
{
  for (uint32_t index = 0U; index < BUTTON_COUNT; index++)
  {
    GPIO_PinState level = HAL_GPIO_ReadPin(button_test_states[index].port,
                                           button_test_states[index].pin);

    button_test_states[index].stable_level = level;
    button_test_states[index].last_raw_level = level;
    button_test_states[index].last_raw_change_ms = now_ms;
    button_test_states[index].pressed_since_ms = 0U;
  }
}

static void ButtonTest_Update(uint32_t now_ms)
{
  char line[64];

  for (uint32_t index = 0U; index < BUTTON_COUNT; index++)
  {
    ButtonTestState *button = &button_test_states[index];
    GPIO_PinState raw_level = HAL_GPIO_ReadPin(button->port, button->pin);

    if (raw_level != button->last_raw_level)
    {
      button->last_raw_level = raw_level;
      button->last_raw_change_ms = now_ms;
    }

    if ((raw_level != button->stable_level) &&
        ((now_ms - button->last_raw_change_ms) >= BUTTON_DEBOUNCE_MS))
    {
      button->stable_level = raw_level;

      if (button->stable_level == GPIO_PIN_RESET)
      {
        button->pressed_since_ms = now_ms;
        (void) snprintf(line, sizeof(line), "%s pressed", button->name);
        Debug_WriteLine(line);
      }
      else
      {
        uint32_t press_time_ms = now_ms - button->pressed_since_ms;
        bool is_long_press = press_time_ms >= BUTTON_LONG_PRESS_MS;
        const char *press_kind = is_long_press ? "long" : "short";

        (void) snprintf(line, sizeof(line), "%s released: %s %lu ms",
                        button->name, press_kind,
                        (unsigned long) press_time_ms);
        Debug_WriteLine(line);

        if (is_long_press)
        {
          continue;
        }

        App_HandleButtonShortPress(index);
      }
    }
  }
}

static uint8_t Brightness_CalculateTargetPercent(uint16_t clear_value)
{
  uint32_t range;
  uint32_t offset;
  uint32_t brightness_range;

  if (clear_value <= BRIGHTNESS_LOW_LIGHT_CLEAR)
  {
    return BRIGHTNESS_MIN_PERCENT;
  }

  if (clear_value >= BRIGHTNESS_HIGH_LIGHT_CLEAR)
  {
    return BRIGHTNESS_MAX_PERCENT;
  }

  range = BRIGHTNESS_HIGH_LIGHT_CLEAR - BRIGHTNESS_LOW_LIGHT_CLEAR;
  offset = clear_value - BRIGHTNESS_LOW_LIGHT_CLEAR;
  brightness_range = BRIGHTNESS_MAX_PERCENT - BRIGHTNESS_MIN_PERCENT;

  return (uint8_t) (BRIGHTNESS_MIN_PERCENT +
                    ((offset * brightness_range) / range));
}

static uint8_t Brightness_StepToward(uint8_t current_percent,
                                     uint8_t target_percent)
{
  if (current_percent < target_percent)
  {
    uint8_t next_percent =
        (uint8_t) (current_percent + BRIGHTNESS_SMOOTH_STEP_PERCENT);

    return (next_percent > target_percent) ? target_percent : next_percent;
  }

  if (current_percent > target_percent)
  {
    if ((current_percent - target_percent) < BRIGHTNESS_SMOOTH_STEP_PERCENT)
    {
      return target_percent;
    }

    return (uint8_t) (current_percent - BRIGHTNESS_SMOOTH_STEP_PERCENT);
  }

  return current_percent;
}

static void Brightness_SetPercent(uint8_t brightness_percent)
{
  uint32_t timer_steps = htim3.Init.Period + 1U;
  uint32_t compare;

  if (brightness_percent > 100U)
  {
    brightness_percent = 100U;
  }

  /* 74HC595 OE is active-low: HIGH disables LEDs, LOW enables LEDs. */
  compare = ((100U - brightness_percent) * timer_steps) / 100U;
  if (compare > htim3.Init.Period)
  {
    compare = htim3.Init.Period;
  }

  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, compare);
}

static uint8_t Display_EncodeBinary7(uint8_t value)
{
  return (uint8_t) ((value & 0x7FU) << 1U);
}

static uint8_t Display_BuildStatus(uint8_t mode_status)
{
  uint8_t status = mode_status;

  if (App_IsEditing())
  {
    status |= DISPLAY_STATUS_EDITING;
    if (!app_edit_blink_on)
    {
      status &= (uint8_t) ~mode_status;
    }
  }
  else if (app_status_feedback_on)
  {
    status |= DISPLAY_STATUS_EDITING;
  }

  if (AlarmService_IsRinging(&alarm_service))
  {
    if (alarm_service.buzzer_active)
    {
      status |= DISPLAY_STATUS_ALARM;
    }
    else
    {
      status &= (uint8_t) ~DISPLAY_STATUS_ALARM;
    }
  }
  else if (alarm_service.enabled && !App_IsEditing())
  {
    status |= DISPLAY_STATUS_ALARM;
  }

  return status;
}

static void Display_WriteRows(uint8_t top, uint8_t bottom, uint8_t status,
                              bool show_top, bool show_bottom)
{
  uint32_t display_value =
      (((uint32_t) Display_BuildStatus(status)) << 16U) |
      (((uint32_t) (show_bottom ? Display_EncodeBinary7(bottom) : 0U)) << 8U) |
      (show_top ? Display_EncodeBinary7(top) : 0U);

  ShiftRegister_Write24(display_value);
}

static void Display_WriteTime(uint8_t hours, uint8_t minutes)
{
  Display_WriteRows(hours, minutes, DISPLAY_STATUS_TIME, true, true);
}

static void Display_WriteDate(uint8_t month, uint8_t day)
{
  Display_WriteRows(month, day, DISPLAY_STATUS_DATE, true, true);
}

static void Display_WriteEnvironment(const Htu21dMeasurement *measurement)
{
  int16_t temperature_centi_c;
  uint16_t humidity_centi_rh;
  uint8_t temperature_c;
  uint8_t humidity_rh;

  if (measurement == NULL)
  {
    return;
  }

  temperature_centi_c = measurement->temperature_centi_c;
  humidity_centi_rh = measurement->humidity_centi_rh;

  if (temperature_centi_c < 0)
  {
    temperature_c = 0U;
  }
  else
  {
    temperature_c = (uint8_t) ((temperature_centi_c + 50) / 100);
  }

  humidity_rh = (uint8_t) ((humidity_centi_rh + 50U) / 100U);

  Display_WriteRows(temperature_c, humidity_rh, DISPLAY_STATUS_ENV, true, true);
}

static void Display_WriteAlarm(void)
{
  Display_WriteRows(alarm_service.hours, alarm_service.minutes,
                    DISPLAY_STATUS_ALARM, true, true);
}

static void Display_WriteEdit(void)
{
  switch (app_edit_field)
  {
    case APP_EDIT_FIELD_TIME_HOURS:
      Display_WriteRows(app_edit_hours, app_edit_minutes, DISPLAY_STATUS_TIME,
                        app_edit_blink_on, true);
      break;

    case APP_EDIT_FIELD_TIME_MINUTES:
      Display_WriteRows(app_edit_hours, app_edit_minutes, DISPLAY_STATUS_TIME,
                        true, app_edit_blink_on);
      break;

    case APP_EDIT_FIELD_DATE_MONTH:
      Display_WriteRows(app_edit_month, app_edit_day, DISPLAY_STATUS_DATE,
                        app_edit_blink_on, true);
      break;

    case APP_EDIT_FIELD_DATE_DAY:
      Display_WriteRows(app_edit_month, app_edit_day, DISPLAY_STATUS_DATE,
                        true, app_edit_blink_on);
      break;

    case APP_EDIT_FIELD_ALARM_HOURS:
      Display_WriteRows(app_edit_hours, app_edit_minutes, DISPLAY_STATUS_ALARM,
                        app_edit_blink_on, true);
      break;

    case APP_EDIT_FIELD_ALARM_MINUTES:
      Display_WriteRows(app_edit_hours, app_edit_minutes, DISPLAY_STATUS_ALARM,
                        true, app_edit_blink_on);
      break;

    case APP_EDIT_FIELD_NONE:
    default:
      break;
  }
}

static void Display_WriteCurrentMode(const RtcDs3231_DateTime *date_time)
{
  if (App_IsEditing())
  {
    Display_WriteEdit();
    return;
  }

  switch (app_display_mode)
  {
    case APP_DISPLAY_MODE_ALARM:
      Display_WriteAlarm();
      break;

    case APP_DISPLAY_MODE_ENV:
      if (latest_env_measurement_valid)
      {
        Display_WriteEnvironment(&latest_env_measurement);
      }
      break;

    case APP_DISPLAY_MODE_DATE:
      if (date_time != NULL)
      {
        Display_WriteDate(date_time->month, date_time->day);
      }
      break;

    case APP_DISPLAY_MODE_TIME:
    default:
      if (date_time != NULL)
      {
        Display_WriteTime(date_time->hours, date_time->minutes);
      }
      break;
  }
}

static const char *App_DisplayModeName(AppDisplayMode mode)
{
  switch (mode)
  {
    case APP_DISPLAY_MODE_ALARM:
      return "ALARM";

    case APP_DISPLAY_MODE_ENV:
      return "ENV";

    case APP_DISPLAY_MODE_DATE:
      return "DATE";

    case APP_DISPLAY_MODE_TIME:
    default:
      return "TIME";
  }
}

static const char *App_GestureName(Apds9960Gesture gesture)
{
  switch (gesture)
  {
    case APDS9960_GESTURE_LEFT:
      return "LEFT";

    case APDS9960_GESTURE_RIGHT:
      return "RIGHT";

    case APDS9960_GESTURE_UP:
      return "UP";

    case APDS9960_GESTURE_DOWN:
      return "DOWN";

    case APDS9960_GESTURE_NONE:
    default:
      return "NONE";
  }
}

static void App_ReadRtcAndRefresh(void)
{
  RtcDs3231_DateTime now;

  if (RtcDs3231_ReadDateTime(&hi2c1, &now) == HAL_OK)
  {
    Debug_WriteDateTime(&now);
    latest_rtc_time = now;
    latest_rtc_time_valid = true;
    if (!App_IsEditing())
    {
      Display_WriteCurrentMode(&latest_rtc_time);
    }
  }
  else
  {
    Debug_WriteLine("RTC read FAILED");
  }
}

static void App_ReadEnvironmentAndRefresh(void)
{
  Htu21dMeasurement measurement;

  if (Htu21d_ReadMeasurement(&hi2c1, &measurement) == HAL_OK)
  {
    Debug_WriteMeasurement(&measurement);
    latest_env_measurement = measurement;
    latest_env_measurement_valid = true;
    if ((app_display_mode == APP_DISPLAY_MODE_ENV) && !App_IsEditing())
    {
      Display_WriteEnvironment(&latest_env_measurement);
    }
  }
  else
  {
    Debug_WriteLine("HTU21D read FAILED");
  }
}

static void App_ReadAmbientLightAndUpdate(uint8_t *current_brightness_percent)
{
  Apds9960AmbientLight light;
  HAL_StatusTypeDef status;

  if (current_brightness_percent == NULL)
  {
    return;
  }

  status = Apds9960_ReadAmbientLight(&hi2c1, &light);
  if (status == HAL_OK)
  {
    uint8_t target_brightness_percent =
        Brightness_CalculateTargetPercent(light.clear);
    char line[72];

    *current_brightness_percent =
        Brightness_StepToward(*current_brightness_percent,
                              target_brightness_percent);
    Brightness_SetPercent(*current_brightness_percent);
    Debug_WriteAmbientLight(&light);
    (void) snprintf(line, sizeof(line),
                    "AUTO BRIGHTNESS target=%u%% current=%u%%",
                    target_brightness_percent,
                    *current_brightness_percent);
    Debug_WriteLine(line);
  }
  else if (status != HAL_BUSY)
  {
    Debug_WriteLine("APDS-9960 light read FAILED");
  }
}

static bool App_IsEditing(void)
{
  return app_edit_field != APP_EDIT_FIELD_NONE;
}

static void App_SetDisplayMode(AppDisplayMode mode)
{
  char line[32];

  app_display_mode = mode;
  (void) snprintf(line, sizeof(line), "DISPLAY MODE %s",
                  App_DisplayModeName(app_display_mode));
  Debug_WriteLine(line);
  Display_WriteCurrentMode(latest_rtc_time_valid ? &latest_rtc_time : NULL);
}

static void App_NextDisplayMode(void)
{
  App_SetDisplayMode((AppDisplayMode) (((uint8_t) app_display_mode + 1U) %
                                       APP_DISPLAY_MODE_COUNT));
}

static void App_PreviousDisplayMode(void)
{
  if (app_display_mode == APP_DISPLAY_MODE_TIME)
  {
    App_SetDisplayMode(APP_DISPLAY_MODE_ALARM);
  }
  else
  {
    App_SetDisplayMode((AppDisplayMode) ((uint8_t) app_display_mode - 1U));
  }
}

static uint8_t App_DaysInMonth(uint16_t year, uint8_t month)
{
  switch (month)
  {
    case 2U:
      if (((year % 4U) == 0U) &&
          (((year % 100U) != 0U) || ((year % 400U) == 0U)))
      {
        return 29U;
      }
      return 28U;

    case 4U:
    case 6U:
    case 9U:
    case 11U:
      return 30U;

    default:
      return 31U;
  }
}

static uint8_t App_WrapValue(uint8_t value, int8_t delta, uint8_t min_value,
                             uint8_t max_value)
{
  int16_t next_value = (int16_t) value + delta;

  if (next_value < (int16_t) min_value)
  {
    return max_value;
  }

  if (next_value > (int16_t) max_value)
  {
    return min_value;
  }

  return (uint8_t) next_value;
}

static void App_StartSetupForCurrentMode(void)
{
  if (app_display_mode == APP_DISPLAY_MODE_ENV)
  {
    Debug_WriteLine("SETUP skipped: ENV has no editable fields");
    return;
  }

  if ((app_display_mode == APP_DISPLAY_MODE_TIME) ||
      (app_display_mode == APP_DISPLAY_MODE_DATE))
  {
    if (!latest_rtc_time_valid)
    {
      Debug_WriteLine("SETUP skipped: RTC time is not ready");
      return;
    }

    app_edit_hours = latest_rtc_time.hours;
    app_edit_minutes = latest_rtc_time.minutes;
    app_edit_month = latest_rtc_time.month;
    app_edit_day = latest_rtc_time.day;
  }

  app_edit_blink_on = true;

  if (app_display_mode == APP_DISPLAY_MODE_TIME)
  {
    app_edit_field = APP_EDIT_FIELD_TIME_HOURS;
    Debug_WriteLine("SETUP TIME: edit hours");
  }
  else if (app_display_mode == APP_DISPLAY_MODE_DATE)
  {
    app_edit_field = APP_EDIT_FIELD_DATE_MONTH;
    Debug_WriteLine("SETUP DATE: edit month");
  }
  else
  {
    app_edit_hours = alarm_service.hours;
    app_edit_minutes = alarm_service.minutes;
    app_edit_field = APP_EDIT_FIELD_ALARM_HOURS;
    Debug_WriteLine("SETUP ALARM: edit hours");
  }

  Display_WriteCurrentMode(latest_rtc_time_valid ? &latest_rtc_time : NULL);
}

static void App_CancelEdit(void)
{
  if (!App_IsEditing())
  {
    return;
  }

  app_edit_field = APP_EDIT_FIELD_NONE;
  app_edit_blink_on = true;
  Debug_WriteLine("SETUP canceled");
  Display_WriteCurrentMode(latest_rtc_time_valid ? &latest_rtc_time : NULL);
}

static void App_AdjustEditValue(int8_t delta)
{
  uint8_t max_day;

  switch (app_edit_field)
  {
    case APP_EDIT_FIELD_TIME_HOURS:
    case APP_EDIT_FIELD_ALARM_HOURS:
      app_edit_hours = App_WrapValue(app_edit_hours, delta, 0U, 23U);
      break;

    case APP_EDIT_FIELD_TIME_MINUTES:
    case APP_EDIT_FIELD_ALARM_MINUTES:
      app_edit_minutes = App_WrapValue(app_edit_minutes, delta, 0U, 59U);
      break;

    case APP_EDIT_FIELD_DATE_MONTH:
      app_edit_month = App_WrapValue(app_edit_month, delta, 1U, 12U);
      max_day = App_DaysInMonth(latest_rtc_time.year, app_edit_month);
      if (app_edit_day > max_day)
      {
        app_edit_day = max_day;
      }
      break;

    case APP_EDIT_FIELD_DATE_DAY:
      max_day = App_DaysInMonth(latest_rtc_time.year, app_edit_month);
      app_edit_day = App_WrapValue(app_edit_day, delta, 1U, max_day);
      break;

    case APP_EDIT_FIELD_NONE:
    default:
      return;
  }

  app_edit_blink_on = true;
  Display_WriteEdit();
}

static void App_AdvanceOrSaveEdit(void)
{
  char line[56];

  switch (app_edit_field)
  {
    case APP_EDIT_FIELD_TIME_HOURS:
      app_edit_field = APP_EDIT_FIELD_TIME_MINUTES;
      app_edit_blink_on = true;
      Debug_WriteLine("SETUP TIME: edit minutes");
      Display_WriteEdit();
      return;

    case APP_EDIT_FIELD_DATE_MONTH:
      app_edit_field = APP_EDIT_FIELD_DATE_DAY;
      app_edit_blink_on = true;
      Debug_WriteLine("SETUP DATE: edit day");
      Display_WriteEdit();
      return;

    case APP_EDIT_FIELD_ALARM_HOURS:
      app_edit_field = APP_EDIT_FIELD_ALARM_MINUTES;
      app_edit_blink_on = true;
      Debug_WriteLine("SETUP ALARM: edit minutes");
      Display_WriteEdit();
      return;

    case APP_EDIT_FIELD_TIME_MINUTES:
      if (latest_rtc_time_valid)
      {
        RtcDs3231_DateTime edited_time = latest_rtc_time;

        edited_time.hours = app_edit_hours;
        edited_time.minutes = app_edit_minutes;
        edited_time.seconds = 0U;

        if (RtcDs3231_WriteDateTime(&hi2c1, &edited_time) == HAL_OK)
        {
          latest_rtc_time = edited_time;
          latest_rtc_time_valid = true;
          app_edit_field = APP_EDIT_FIELD_NONE;
          (void) snprintf(line, sizeof(line), "TIME saved %02u:%02u",
                          edited_time.hours, edited_time.minutes);
          Debug_WriteLine(line);
          Display_WriteCurrentMode(&latest_rtc_time);
        }
        else
        {
          Debug_WriteLine("TIME save FAILED");
        }
      }
      return;

    case APP_EDIT_FIELD_DATE_DAY:
      if (latest_rtc_time_valid)
      {
        RtcDs3231_DateTime edited_date = latest_rtc_time;

        edited_date.month = app_edit_month;
        edited_date.day = app_edit_day;

        if (RtcDs3231_WriteDateTime(&hi2c1, &edited_date) == HAL_OK)
        {
          latest_rtc_time = edited_date;
          latest_rtc_time_valid = true;
          app_edit_field = APP_EDIT_FIELD_NONE;
          (void) snprintf(line, sizeof(line), "DATE saved %02u-%02u",
                          edited_date.month, edited_date.day);
          Debug_WriteLine(line);
          Display_WriteCurrentMode(&latest_rtc_time);
        }
        else
        {
          Debug_WriteLine("DATE save FAILED");
        }
      }
      return;

    case APP_EDIT_FIELD_ALARM_MINUTES:
      if (AlarmService_SetAlarm(&alarm_service, app_edit_hours,
                                app_edit_minutes,
                                alarm_service.enabled) == HAL_OK)
      {
        app_edit_field = APP_EDIT_FIELD_NONE;
        (void) snprintf(line, sizeof(line), "ALARM saved %02u:%02u %s",
                        alarm_service.hours, alarm_service.minutes,
                        alarm_service.enabled ? "enabled" : "disabled");
        Debug_WriteLine(line);
        Display_WriteCurrentMode(latest_rtc_time_valid ? &latest_rtc_time : NULL);
      }
      else
      {
        Debug_WriteLine("ALARM save FAILED");
      }
      return;

    case APP_EDIT_FIELD_NONE:
    default:
      return;
  }
}

static void App_HandleButtonShortPress(uint32_t button_index)
{
  if (button_index == BUTTON_MODE_INDEX)
  {
    if (App_IsEditing())
    {
      App_CancelEdit();
    }
    else
    {
      App_StartSetupForCurrentMode();
    }
    return;
  }

  if (App_IsEditing())
  {
    if (button_index == BUTTON_PLUS_INDEX)
    {
      App_AdjustEditValue(1);
    }
    else if (button_index == BUTTON_MINUS_INDEX)
    {
      App_AdjustEditValue(-1);
    }
    else if (button_index == BUTTON_OK_INDEX)
    {
      App_AdvanceOrSaveEdit();
    }
    return;
  }

  if (button_index == BUTTON_PLUS_INDEX)
  {
    App_NextDisplayMode();
  }
  else if (button_index == BUTTON_MINUS_INDEX)
  {
    App_PreviousDisplayMode();
  }
  else if (button_index == BUTTON_OK_INDEX)
  {
    App_HandleOkButtonShortPress();
  }
}

static void App_HandleOkButtonShortPress(void)
{
  char line[48];
  bool enabled;

  if (AlarmService_IsRinging(&alarm_service))
  {
    if (AlarmService_Dismiss(&alarm_service) == HAL_OK)
    {
      Debug_WriteLine("ALARM dismissed by OK");
      Display_WriteCurrentMode(latest_rtc_time_valid ? &latest_rtc_time : NULL);
    }
    else
    {
      Debug_WriteLine("ALARM dismiss FAILED");
    }
    return;
  }

  if (app_display_mode != APP_DISPLAY_MODE_ALARM)
  {
    return;
  }

  enabled = !alarm_service.enabled;
  if (AlarmService_SetAlarm(&alarm_service, alarm_service.hours,
                            alarm_service.minutes, enabled) == HAL_OK)
  {
    AlarmService_PlayToggleFeedback(&alarm_service, alarm_service.enabled);
    app_status_feedback_ticks = 2U;
    app_status_feedback_on = true;
    (void) snprintf(line, sizeof(line), "ALARM %02u:%02u %s",
                    alarm_service.hours, alarm_service.minutes,
                    alarm_service.enabled ? "enabled" : "disabled");
    Debug_WriteLine(line);
    Display_WriteAlarm();
  }
  else
  {
    Debug_WriteLine("ALARM toggle FAILED");
  }
}

static bool App_IsAlarmRinging(void)
{
  return AlarmService_IsRinging(&alarm_service);
}

static void App_SnoozeAlarmOneMinute(void)
{
  if (latest_rtc_time_valid)
  {
    uint16_t total_minutes =
        (uint16_t) latest_rtc_time.hours * 60U + latest_rtc_time.minutes + 1U;

    total_minutes %= (24U * 60U);

    if (AlarmService_SetAlarm(&alarm_service, (uint8_t) (total_minutes / 60U),
                              (uint8_t) (total_minutes % 60U),
                              true) == HAL_OK)
    {
      Debug_WriteLine("ALARM snoozed 1 minute by gesture");
      Display_WriteCurrentMode(latest_rtc_time_valid ? &latest_rtc_time : NULL);
      return;
    }
  }

  Debug_WriteLine("ALARM snooze FAILED");
}

static void App_DismissAlarm(void)
{
  if (AlarmService_Dismiss(&alarm_service) == HAL_OK)
  {
    Debug_WriteLine("ALARM dismissed by gesture");
    Display_WriteCurrentMode(latest_rtc_time_valid ? &latest_rtc_time : NULL);
  }
  else
  {
    Debug_WriteLine("ALARM dismiss FAILED");
  }
}

static void App_HandleGesture(Apds9960Gesture gesture, uint32_t now_ms)
{
  static Apds9960Gesture pending_alarm_gesture = APDS9960_GESTURE_NONE;
  static uint32_t pending_alarm_gesture_ms = 0U;
  char line[32];

  if (gesture == APDS9960_GESTURE_NONE)
  {
    return;
  }

  (void) snprintf(line, sizeof(line), "GESTURE %s", App_GestureName(gesture));
  Debug_WriteLine(line);

  if (!App_IsAlarmRinging() && App_IsEditing())
  {
    return;
  }

  if (!App_IsAlarmRinging())
  {
    pending_alarm_gesture = APDS9960_GESTURE_NONE;

    if ((gesture == APDS9960_GESTURE_RIGHT) ||
        (gesture == APDS9960_GESTURE_DOWN))
    {
      App_NextDisplayMode();
    }
    else if ((gesture == APDS9960_GESTURE_LEFT) ||
             (gesture == APDS9960_GESTURE_UP))
    {
      App_PreviousDisplayMode();
    }

    return;
  }

  if ((pending_alarm_gesture != APDS9960_GESTURE_NONE) &&
      ((now_ms - pending_alarm_gesture_ms) >
       GESTURE_ALARM_SEQUENCE_TIMEOUT_MS))
  {
    pending_alarm_gesture = APDS9960_GESTURE_NONE;
  }

  if ((pending_alarm_gesture == APDS9960_GESTURE_LEFT) &&
      (gesture == APDS9960_GESTURE_RIGHT))
  {
    pending_alarm_gesture = APDS9960_GESTURE_NONE;
    App_SnoozeAlarmOneMinute();
    return;
  }

  if ((pending_alarm_gesture == APDS9960_GESTURE_RIGHT) &&
      (gesture == APDS9960_GESTURE_LEFT))
  {
    pending_alarm_gesture = APDS9960_GESTURE_NONE;
    App_DismissAlarm();
    return;
  }

  if ((gesture == APDS9960_GESTURE_LEFT) ||
      (gesture == APDS9960_GESTURE_RIGHT))
  {
    pending_alarm_gesture = gesture;
    pending_alarm_gesture_ms = now_ms;
  }
}

static void ShiftRegister_Write24(uint32_t value)
{
  uint8_t bytes[SHIFT_REGISTER_BYTE_COUNT];

  bytes[0] = (uint8_t) ((value >> 16U) & 0xFFU);
  bytes[1] = (uint8_t) ((value >> 8U) & 0xFFU);
  bytes[2] = (uint8_t) (value & 0xFFU);

  HAL_GPIO_WritePin(LATCH_595_GPIO_Port, LATCH_595_Pin, GPIO_PIN_RESET);
  (void) HAL_SPI_Transmit(&hspi1, bytes, SHIFT_REGISTER_BYTE_COUNT,
                          SHIFT_REGISTER_TIMEOUT_MS);
  HAL_GPIO_WritePin(LATCH_595_GPIO_Port, LATCH_595_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LATCH_595_GPIO_Port, LATCH_595_Pin, GPIO_PIN_RESET);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  (void) HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  Brightness_SetPercent(BRIGHTNESS_MAX_PERCENT);

  ShiftRegister_Write24(0U);
  ButtonTest_Init(0U);
  AlarmService_Init(&alarm_service, &hi2c1, BUZZER_GPIO_Port, BUZZER_Pin);
  Debug_WriteLine("binary_clock_f103 bring-up started");
  Debug_WriteLine("Display modes: TIME -> DATE -> ENV -> ALARM");
  Debug_WriteLine("UI: PLUS/MINUS change mode, MODE enters setup, OK confirms");
  Debug_WriteLine("Auto brightness enabled from APDS-9960 clear channel");
  Debug_WriteLine("Button test enabled: PA0 SETUP, PA1 PLUS, PA2 MINUS, PA3 OK");

  if (!RtcDs3231_IsReady(&hi2c1))
  {
    Debug_WriteLine("DS3231 not found on I2C1");
  }
  else
  {
    Debug_WriteLine("DS3231 found");

#if RTC_WRITE_EXAMPLE_ON_BOOT
    const RtcDs3231_DateTime start_time = {
      .year = 2026U,
      .month = 8U,
      .day = 18U,
      .weekday = 2U,
      .hours = 12U,
      .minutes = 0U,
      .seconds = 0U,
    };

    if (RtcDs3231_WriteDateTime(&hi2c1, &start_time) == HAL_OK)
    {
      Debug_WriteLine("RTC write OK");
    }
    else
    {
      Debug_WriteLine("RTC write FAILED");
    }
#endif
    if (AlarmService_SetAlarm(&alarm_service, alarm_service.hours,
                              alarm_service.minutes,
                              alarm_service.enabled) == HAL_OK)
    {
      Debug_WriteLine("Alarm 07:30 disabled");
    }
    else
    {
      Debug_WriteLine("Alarm init FAILED");
    }

    App_ReadRtcAndRefresh();
  }

  if (Htu21d_IsReady(&hi2c1))
  {
    Debug_WriteLine("HTU21D found");
    App_ReadEnvironmentAndRefresh();
  }
  else
  {
    Debug_WriteLine("HTU21D not found on I2C1");
  }

  if (Apds9960_IsReady(&hi2c1))
  {
    uint8_t id = 0U;
    char line[40];

    if (Apds9960_ReadId(&hi2c1, &id) == HAL_OK)
    {
      (void) snprintf(line, sizeof(line), "APDS-9960 found, ID 0x%02X", id);
      Debug_WriteLine(line);
    }
    else
    {
      Debug_WriteLine("APDS-9960 found, ID read FAILED");
    }

    if (Apds9960_InitAmbientLight(&hi2c1) == HAL_OK)
    {
      Debug_WriteLine("APDS-9960 ambient light enabled");
    }
    else
    {
      Debug_WriteLine("APDS-9960 ambient light init FAILED");
    }

    if (Apds9960_InitGesture(&hi2c1) == HAL_OK)
    {
      Debug_WriteLine("APDS-9960 gesture enabled");
    }
    else
    {
      Debug_WriteLine("APDS-9960 gesture init FAILED");
    }
  }
  else
  {
    Debug_WriteLine("APDS-9960 not found on I2C1");
  }

  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static uint8_t current_brightness_percent = BRIGHTNESS_MAX_PERCENT;
    static bool soft_alarm_by_proximity = false;

    while (AppScheduler_Consume10msTick())
    {
      uint32_t now_ms = AppScheduler_NowMs();

      ButtonTest_Update(now_ms);
      AlarmService_Process(&alarm_service);

      if (AlarmService_OnTick(&alarm_service, 10U) &&
          AlarmService_IsRinging(&alarm_service))
      {
        Display_WriteCurrentMode(latest_rtc_time_valid ? &latest_rtc_time : NULL);
      }
    }

    if (AppScheduler_Consume500msTick())
    {
      bool refresh_display = AlarmService_IsRinging(&alarm_service) ||
                             App_IsEditing() ||
                             (app_status_feedback_ticks > 0U);

      HAL_GPIO_TogglePin(BOARD_LED_GPIO_Port, BOARD_LED_Pin);
      app_edit_blink_on = !app_edit_blink_on;

      if (app_status_feedback_ticks > 0U)
      {
        app_status_feedback_ticks--;
        app_status_feedback_on = false;
      }

      if (refresh_display)
      {
        Display_WriteCurrentMode(latest_rtc_time_valid ? &latest_rtc_time : NULL);
      }
    }

    if (AppScheduler_Consume1sTick())
    {
      App_ReadRtcAndRefresh();
      App_ReadAmbientLightAndUpdate(&current_brightness_percent);
    }

    if (AppScheduler_Consume120msTick())
    {
      Apds9960Gesture gesture;
      HAL_StatusTypeDef status;

      status = Apds9960_ReadGesture(&hi2c1, &gesture);
      if (status == HAL_OK)
      {
        App_HandleGesture(gesture, AppScheduler_NowMs());
      }
      else if (status != HAL_BUSY)
      {
        Debug_WriteLine("APDS-9960 gesture read FAILED");
      }

      if (AlarmService_IsRinging(&alarm_service))
      {
        uint8_t proximity;

        status = Apds9960_ReadProximity(&hi2c1, &proximity);
        if (status == HAL_OK)
        {
          if (!soft_alarm_by_proximity &&
              (proximity >= PROXIMITY_SOFT_ALARM_ON))
          {
            char line[48];

            soft_alarm_by_proximity = true;
            AlarmService_SetSoftRinging(&alarm_service, true);
            (void) snprintf(line, sizeof(line), "ALARM soft proximity=%u",
                            proximity);
            Debug_WriteLine(line);
          }
          else if (soft_alarm_by_proximity &&
                   (proximity <= PROXIMITY_SOFT_ALARM_OFF))
          {
            char line[48];

            soft_alarm_by_proximity = false;
            AlarmService_SetSoftRinging(&alarm_service, false);
            (void) snprintf(line, sizeof(line), "ALARM normal proximity=%u",
                            proximity);
            Debug_WriteLine(line);
          }
        }
        else if (status != HAL_BUSY)
        {
          Debug_WriteLine("APDS-9960 proximity read FAILED");
        }
      }
      else if (soft_alarm_by_proximity)
      {
        soft_alarm_by_proximity = false;
        AlarmService_SetSoftRinging(&alarm_service, false);
      }
    }

    if (AppScheduler_Consume60sTick())
    {
      App_ReadEnvironmentAndRefresh();
    }

    __WFI();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 9;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 7;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BOARD_LED_GPIO_Port, BOARD_LED_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LATCH_595_GPIO_Port, LATCH_595_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : BOARD_LED_Pin */
  GPIO_InitStruct.Pin = BOARD_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BOARD_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_MODE_Pin BTN_PLUS_Pin BTN_MINUS_Pin BTN_OK_Pin */
  GPIO_InitStruct.Pin = BTN_MODE_Pin|BTN_PLUS_Pin|BTN_MINUS_Pin|BTN_OK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LATCH_595_Pin */
  GPIO_InitStruct.Pin = LATCH_595_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LATCH_595_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DS3231_INT_Pin APDS_INT_Pin */
  GPIO_InitStruct.Pin = DS3231_INT_Pin|APDS_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BUZZER_Pin */
  GPIO_InitStruct.Pin = BUZZER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if ((htim != NULL) && (htim->Instance == TIM2))
  {
    AppScheduler_On10msTickFromIsr();
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == DS3231_INT_Pin)
  {
    AlarmService_HandleRtcInterruptFromIsr(&alarm_service);
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
