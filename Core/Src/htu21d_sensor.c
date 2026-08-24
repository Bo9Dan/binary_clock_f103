#include "htu21d_sensor.h"

#define HTU21D_I2C_ADDRESS (0x40U << 1U)
#define HTU21D_COMMAND_READ_TEMP_NO_HOLD 0xF3U
#define HTU21D_COMMAND_READ_HUM_NO_HOLD 0xF5U
#define HTU21D_I2C_TIMEOUT_MS 200U
#define HTU21D_TEMP_WAIT_MS 85U
#define HTU21D_HUM_WAIT_MS 35U

static uint8_t Htu21d_Crc8(const uint8_t bytes[2])
{
  uint8_t crc = 0U;

  for (uint8_t byte_index = 0U; byte_index < 2U; byte_index++)
  {
    crc ^= bytes[byte_index];

    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x80U) != 0U)
      {
        crc = (uint8_t) ((crc << 1U) ^ 0x31U);
      }
      else
      {
        crc = (uint8_t) (crc << 1U);
      }
    }
  }

  return crc;
}

static HAL_StatusTypeDef Htu21d_ReadRaw(I2C_HandleTypeDef *hi2c,
                                        uint8_t command,
                                        uint32_t wait_ms,
                                        uint16_t *raw_value)
{
  uint8_t bytes[3];
  HAL_StatusTypeDef status;

  if ((hi2c == NULL) || (raw_value == NULL))
  {
    return HAL_ERROR;
  }

  status = HAL_I2C_Master_Transmit(hi2c, HTU21D_I2C_ADDRESS, &command, 1U,
                                   HTU21D_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  HAL_Delay(wait_ms);

  status = HAL_I2C_Master_Receive(hi2c, HTU21D_I2C_ADDRESS, bytes,
                                  sizeof(bytes), HTU21D_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  if (Htu21d_Crc8(bytes) != bytes[2])
  {
    return HAL_ERROR;
  }

  *raw_value = (uint16_t) ((((uint16_t) bytes[0]) << 8U) | bytes[1]);
  *raw_value &= 0xFFFCU;

  return HAL_OK;
}

bool Htu21d_IsReady(I2C_HandleTypeDef *hi2c)
{
  return HAL_I2C_IsDeviceReady(hi2c, HTU21D_I2C_ADDRESS, 3U,
                               HTU21D_I2C_TIMEOUT_MS) == HAL_OK;
}

HAL_StatusTypeDef Htu21d_ReadMeasurement(I2C_HandleTypeDef *hi2c,
                                         Htu21dMeasurement *measurement)
{
  uint16_t raw_temperature;
  uint16_t raw_humidity;
  int32_t temperature_centi_c;
  int32_t humidity_centi_rh;
  HAL_StatusTypeDef status;

  if (measurement == NULL)
  {
    return HAL_ERROR;
  }

  status = Htu21d_ReadRaw(hi2c, HTU21D_COMMAND_READ_TEMP_NO_HOLD,
                          HTU21D_TEMP_WAIT_MS, &raw_temperature);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Htu21d_ReadRaw(hi2c, HTU21D_COMMAND_READ_HUM_NO_HOLD,
                          HTU21D_HUM_WAIT_MS, &raw_humidity);
  if (status != HAL_OK)
  {
    return status;
  }

  temperature_centi_c =
      -4685L + (int32_t) ((((int64_t) 17572L * raw_temperature) + 32768L) /
                          65536L);
  humidity_centi_rh =
      -600L + (int32_t) ((((int64_t) 12500L * raw_humidity) + 32768L) /
                         65536L);

  if (humidity_centi_rh < 0L)
  {
    humidity_centi_rh = 0L;
  }
  else if (humidity_centi_rh > 10000L)
  {
    humidity_centi_rh = 10000L;
  }

  measurement->temperature_centi_c = (int16_t) temperature_centi_c;
  measurement->humidity_centi_rh = (uint16_t) humidity_centi_rh;

  return HAL_OK;
}
