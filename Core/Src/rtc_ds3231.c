#include "rtc_ds3231.h"

#define DS3231_I2C_ADDRESS (0x68U << 1U)
#define DS3231_REGISTER_SECONDS 0x00U
#define DS3231_REGISTER_COUNT 7U
#define DS3231_REGISTER_ALARM1_SECONDS 0x07U
#define DS3231_REGISTER_CONTROL 0x0EU
#define DS3231_REGISTER_STATUS 0x0FU
#define DS3231_I2C_TIMEOUT_MS 100U

#define DS3231_CONTROL_A1IE 0x01U
#define DS3231_CONTROL_INTCN 0x04U
#define DS3231_STATUS_A1F 0x01U
#define DS3231_STATUS_OSF 0x80U
#define DS3231_ALARM_MASK_IGNORE_DAY_DATE 0x80U

static uint8_t BcdToUint(uint8_t value)
{
  return (uint8_t) (((value >> 4U) * 10U) + (value & 0x0FU));
}

static uint8_t UintToBcd(uint8_t value)
{
  return (uint8_t) (((value / 10U) << 4U) | (value % 10U));
}

static bool IsDateTimeValid(const RtcDs3231_DateTime *date_time)
{
  if (date_time == NULL)
  {
    return false;
  }

  return (date_time->year >= 2000U) && (date_time->year <= 2099U) &&
         (date_time->month >= 1U) && (date_time->month <= 12U) &&
         (date_time->day >= 1U) && (date_time->day <= 31U) &&
         (date_time->weekday >= 1U) && (date_time->weekday <= 7U) &&
         (date_time->hours <= 23U) && (date_time->minutes <= 59U) &&
         (date_time->seconds <= 59U);
}

static bool IsTimeValid(const RtcDs3231_Time *time)
{
  if (time == NULL)
  {
    return false;
  }

  return (time->hours <= 23U) && (time->minutes <= 59U) &&
         (time->seconds <= 59U);
}

static HAL_StatusTypeDef ReadRegister(I2C_HandleTypeDef *hi2c, uint8_t reg,
                                      uint8_t *value)
{
  if ((hi2c == NULL) || (value == NULL))
  {
    return HAL_ERROR;
  }

  return HAL_I2C_Mem_Read(hi2c, DS3231_I2C_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT,
                          value, 1U, DS3231_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef WriteRegister(I2C_HandleTypeDef *hi2c, uint8_t reg,
                                       uint8_t value)
{
  return HAL_I2C_Mem_Write(hi2c, DS3231_I2C_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT,
                           &value, 1U, DS3231_I2C_TIMEOUT_MS);
}

bool RtcDs3231_IsReady(I2C_HandleTypeDef *hi2c)
{
  return HAL_I2C_IsDeviceReady(hi2c, DS3231_I2C_ADDRESS, 3U,
                               DS3231_I2C_TIMEOUT_MS) == HAL_OK;
}

HAL_StatusTypeDef RtcDs3231_ReadDateTime(I2C_HandleTypeDef *hi2c,
                                         RtcDs3231_DateTime *date_time)
{
  uint8_t registers[DS3231_REGISTER_COUNT];
  HAL_StatusTypeDef status;

  if ((hi2c == NULL) || (date_time == NULL))
  {
    return HAL_ERROR;
  }

  status = HAL_I2C_Mem_Read(hi2c, DS3231_I2C_ADDRESS, DS3231_REGISTER_SECONDS,
                            I2C_MEMADD_SIZE_8BIT, registers, sizeof(registers),
                            DS3231_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  date_time->seconds = BcdToUint(registers[0] & 0x7FU);
  date_time->minutes = BcdToUint(registers[1] & 0x7FU);

  if ((registers[2] & 0x40U) != 0U)
  {
    uint8_t hour = BcdToUint(registers[2] & 0x1FU);
    bool is_pm = (registers[2] & 0x20U) != 0U;

    if (hour == 12U)
    {
      hour = 0U;
    }
    date_time->hours = (uint8_t) (hour + (is_pm ? 12U : 0U));
  }
  else
  {
    date_time->hours = BcdToUint(registers[2] & 0x3FU);
  }

  date_time->weekday = BcdToUint(registers[3] & 0x07U);
  date_time->day = BcdToUint(registers[4] & 0x3FU);
  date_time->month = BcdToUint(registers[5] & 0x1FU);
  date_time->year = (uint16_t) (2000U + BcdToUint(registers[6]));

  return IsDateTimeValid(date_time) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef RtcDs3231_WriteDateTime(I2C_HandleTypeDef *hi2c,
                                          const RtcDs3231_DateTime *date_time)
{
  uint8_t registers[DS3231_REGISTER_COUNT];

  if ((hi2c == NULL) || !IsDateTimeValid(date_time))
  {
    return HAL_ERROR;
  }

  registers[0] = UintToBcd(date_time->seconds);
  registers[1] = UintToBcd(date_time->minutes);
  registers[2] = UintToBcd(date_time->hours);
  registers[3] = UintToBcd(date_time->weekday);
  registers[4] = UintToBcd(date_time->day);
  registers[5] = UintToBcd(date_time->month);
  registers[6] = UintToBcd((uint8_t) (date_time->year - 2000U));

  return HAL_I2C_Mem_Write(hi2c, DS3231_I2C_ADDRESS, DS3231_REGISTER_SECONDS,
                           I2C_MEMADD_SIZE_8BIT, registers, sizeof(registers),
                           DS3231_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef RtcDs3231_IsOscillatorStopFlagSet(I2C_HandleTypeDef *hi2c,
                                                    bool *is_set)
{
  uint8_t status_reg;
  HAL_StatusTypeDef status;

  if (is_set == NULL)
  {
    return HAL_ERROR;
  }

  status = ReadRegister(hi2c, DS3231_REGISTER_STATUS, &status_reg);
  if (status != HAL_OK)
  {
    return status;
  }

  *is_set = (status_reg & DS3231_STATUS_OSF) != 0U;
  return HAL_OK;
}

HAL_StatusTypeDef RtcDs3231_ClearOscillatorStopFlag(I2C_HandleTypeDef *hi2c)
{
  uint8_t status_reg;
  HAL_StatusTypeDef status;

  status = ReadRegister(hi2c, DS3231_REGISTER_STATUS, &status_reg);
  if (status != HAL_OK)
  {
    return status;
  }

  status_reg &= (uint8_t) ~DS3231_STATUS_OSF;
  return WriteRegister(hi2c, DS3231_REGISTER_STATUS, status_reg);
}

HAL_StatusTypeDef RtcDs3231_ReadTime(I2C_HandleTypeDef *hi2c,
                                     RtcDs3231_Time *time)
{
  RtcDs3231_DateTime date_time;
  HAL_StatusTypeDef status;

  if (time == NULL)
  {
    return HAL_ERROR;
  }

  status = RtcDs3231_ReadDateTime(hi2c, &date_time);
  if (status != HAL_OK)
  {
    return status;
  }

  time->hours = date_time.hours;
  time->minutes = date_time.minutes;
  time->seconds = date_time.seconds;

  return HAL_OK;
}

HAL_StatusTypeDef RtcDs3231_WriteTime(I2C_HandleTypeDef *hi2c,
                                      const RtcDs3231_Time *time)
{
  uint8_t registers[3];

  if ((hi2c == NULL) || !IsTimeValid(time))
  {
    return HAL_ERROR;
  }

  registers[0] = UintToBcd(time->seconds);
  registers[1] = UintToBcd(time->minutes);
  registers[2] = UintToBcd(time->hours);

  return HAL_I2C_Mem_Write(hi2c, DS3231_I2C_ADDRESS, DS3231_REGISTER_SECONDS,
                           I2C_MEMADD_SIZE_8BIT, registers, sizeof(registers),
                           DS3231_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef RtcDs3231_SetAlarm1Daily(I2C_HandleTypeDef *hi2c,
                                           uint8_t hours, uint8_t minutes,
                                           uint8_t seconds)
{
  uint8_t registers[4];

  if ((hi2c == NULL) || (hours > 23U) || (minutes > 59U) || (seconds > 59U))
  {
    return HAL_ERROR;
  }

  registers[0] = UintToBcd(seconds);
  registers[1] = UintToBcd(minutes);
  registers[2] = UintToBcd(hours);
  registers[3] = DS3231_ALARM_MASK_IGNORE_DAY_DATE;

  return HAL_I2C_Mem_Write(hi2c, DS3231_I2C_ADDRESS,
                           DS3231_REGISTER_ALARM1_SECONDS,
                           I2C_MEMADD_SIZE_8BIT, registers, sizeof(registers),
                           DS3231_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef RtcDs3231_EnableAlarm1Interrupt(I2C_HandleTypeDef *hi2c,
                                                  bool enabled)
{
  uint8_t control;
  HAL_StatusTypeDef status;

  status = ReadRegister(hi2c, DS3231_REGISTER_CONTROL, &control);
  if (status != HAL_OK)
  {
    return status;
  }

  control |= DS3231_CONTROL_INTCN;
  if (enabled)
  {
    control |= DS3231_CONTROL_A1IE;
  }
  else
  {
    control &= (uint8_t) ~DS3231_CONTROL_A1IE;
  }

  return WriteRegister(hi2c, DS3231_REGISTER_CONTROL, control);
}

HAL_StatusTypeDef RtcDs3231_ClearAlarm1Flag(I2C_HandleTypeDef *hi2c)
{
  uint8_t status_reg;
  HAL_StatusTypeDef status;

  status = ReadRegister(hi2c, DS3231_REGISTER_STATUS, &status_reg);
  if (status != HAL_OK)
  {
    return status;
  }

  status_reg &= (uint8_t) ~DS3231_STATUS_A1F;

  return WriteRegister(hi2c, DS3231_REGISTER_STATUS, status_reg);
}

HAL_StatusTypeDef RtcDs3231_IsAlarm1FlagSet(I2C_HandleTypeDef *hi2c,
                                            bool *is_set)
{
  uint8_t status_reg;
  HAL_StatusTypeDef status;

  if (is_set == NULL)
  {
    return HAL_ERROR;
  }

  status = ReadRegister(hi2c, DS3231_REGISTER_STATUS, &status_reg);
  if (status != HAL_OK)
  {
    return status;
  }

  *is_set = (status_reg & DS3231_STATUS_A1F) != 0U;

  return HAL_OK;
}
