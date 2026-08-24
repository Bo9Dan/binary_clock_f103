#include "apds9960_sensor.h"

#define APDS9960_I2C_ADDRESS (0x39U << 1U)
#define APDS9960_I2C_TIMEOUT_MS 100U

#define APDS9960_REGISTER_ENABLE 0x80U
#define APDS9960_REGISTER_ATIME 0x81U
#define APDS9960_REGISTER_WTIME 0x83U
#define APDS9960_REGISTER_PPULSE 0x8EU
#define APDS9960_REGISTER_CONTROL 0x8FU
#define APDS9960_REGISTER_CONFIG2 0x90U
#define APDS9960_REGISTER_ID 0x92U
#define APDS9960_REGISTER_STATUS 0x93U
#define APDS9960_REGISTER_CDATAL 0x94U
#define APDS9960_REGISTER_PDATA 0x9CU
#define APDS9960_REGISTER_GPENTH 0xA0U
#define APDS9960_REGISTER_GEXTH 0xA1U
#define APDS9960_REGISTER_GCONF1 0xA2U
#define APDS9960_REGISTER_GCONF2 0xA3U
#define APDS9960_REGISTER_GPULSE 0xA6U
#define APDS9960_REGISTER_GCONF3 0xAAU
#define APDS9960_REGISTER_GCONF4 0xABU
#define APDS9960_REGISTER_GFLVL 0xAEU
#define APDS9960_REGISTER_GSTATUS 0xAFU
#define APDS9960_REGISTER_GFIFO_U 0xFCU

#define APDS9960_ENABLE_PON 0x01U
#define APDS9960_ENABLE_AEN 0x02U
#define APDS9960_ENABLE_PEN 0x04U
#define APDS9960_ENABLE_WEN 0x08U
#define APDS9960_ENABLE_GEN 0x40U
#define APDS9960_STATUS_AVALID 0x01U
#define APDS9960_STATUS_PVALID 0x02U
#define APDS9960_GSTATUS_GVALID 0x01U

#define APDS9960_ATIME_100_MS 0xDBU
#define APDS9960_AGAIN_4X 0x01U
#define APDS9960_GESTURE_FIFO_MAX_DATASET 32U
#define APDS9960_GESTURE_MIN_TOTAL 30U
#define APDS9960_GESTURE_RATIO_THRESHOLD 30

static HAL_StatusTypeDef Apds9960_ReadRegister(I2C_HandleTypeDef *hi2c,
                                               uint8_t reg, uint8_t *value)
{
  if ((hi2c == NULL) || (value == NULL))
  {
    return HAL_ERROR;
  }

  return HAL_I2C_Mem_Read(hi2c, APDS9960_I2C_ADDRESS, reg,
                          I2C_MEMADD_SIZE_8BIT, value, 1U,
                          APDS9960_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef Apds9960_WriteRegister(I2C_HandleTypeDef *hi2c,
                                                uint8_t reg, uint8_t value)
{
  if (hi2c == NULL)
  {
    return HAL_ERROR;
  }

  return HAL_I2C_Mem_Write(hi2c, APDS9960_I2C_ADDRESS, reg,
                           I2C_MEMADD_SIZE_8BIT, &value, 1U,
                           APDS9960_I2C_TIMEOUT_MS);
}

static int16_t GestureRatio(uint8_t positive, uint8_t negative)
{
  uint16_t total = (uint16_t) positive + negative;

  if (total == 0U)
  {
    return 0;
  }

  return (int16_t) ((((int16_t) positive - (int16_t) negative) * 100) /
                    (int16_t) total);
}

static Apds9960Gesture DecodeGesture(uint8_t first_u, uint8_t first_d,
                                     uint8_t first_l, uint8_t first_r,
                                     uint8_t last_u, uint8_t last_d,
                                     uint8_t last_l, uint8_t last_r)
{
  int16_t ud_delta = GestureRatio(last_u, last_d) -
                     GestureRatio(first_u, first_d);
  int16_t lr_delta = GestureRatio(last_l, last_r) -
                     GestureRatio(first_l, first_r);
  int16_t abs_ud_delta = (ud_delta < 0) ? (int16_t) -ud_delta : ud_delta;
  int16_t abs_lr_delta = (lr_delta < 0) ? (int16_t) -lr_delta : lr_delta;

  if ((abs_ud_delta < APDS9960_GESTURE_RATIO_THRESHOLD) &&
      (abs_lr_delta < APDS9960_GESTURE_RATIO_THRESHOLD))
  {
    return APDS9960_GESTURE_NONE;
  }

  if (abs_lr_delta > abs_ud_delta)
  {
    return (lr_delta > 0) ? APDS9960_GESTURE_RIGHT
                          : APDS9960_GESTURE_LEFT;
  }

  return (ud_delta > 0) ? APDS9960_GESTURE_DOWN : APDS9960_GESTURE_UP;
}

bool Apds9960_IsReady(I2C_HandleTypeDef *hi2c)
{
  return HAL_I2C_IsDeviceReady(hi2c, APDS9960_I2C_ADDRESS, 3U,
                               APDS9960_I2C_TIMEOUT_MS) == HAL_OK;
}

HAL_StatusTypeDef Apds9960_ReadId(I2C_HandleTypeDef *hi2c, uint8_t *id)
{
  return Apds9960_ReadRegister(hi2c, APDS9960_REGISTER_ID, id);
}

HAL_StatusTypeDef Apds9960_InitAmbientLight(I2C_HandleTypeDef *hi2c)
{
  HAL_StatusTypeDef status;

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_ENABLE, 0U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_ATIME,
                                  APDS9960_ATIME_100_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_CONTROL,
                                  APDS9960_AGAIN_4X);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_ENABLE,
                                  APDS9960_ENABLE_PON);
  if (status != HAL_OK)
  {
    return status;
  }

  HAL_Delay(10U);

  return Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_ENABLE,
                                APDS9960_ENABLE_PON | APDS9960_ENABLE_AEN);
}

HAL_StatusTypeDef Apds9960_ReadAmbientLight(I2C_HandleTypeDef *hi2c,
                                            Apds9960AmbientLight *light)
{
  uint8_t status;
  uint8_t bytes[8];
  HAL_StatusTypeDef result;

  if ((hi2c == NULL) || (light == NULL))
  {
    return HAL_ERROR;
  }

  result = Apds9960_ReadRegister(hi2c, APDS9960_REGISTER_STATUS, &status);
  if (result != HAL_OK)
  {
    return result;
  }

  if ((status & APDS9960_STATUS_AVALID) == 0U)
  {
    return HAL_BUSY;
  }

  result = HAL_I2C_Mem_Read(hi2c, APDS9960_I2C_ADDRESS,
                            APDS9960_REGISTER_CDATAL, I2C_MEMADD_SIZE_8BIT,
                            bytes, sizeof(bytes), APDS9960_I2C_TIMEOUT_MS);
  if (result != HAL_OK)
  {
    return result;
  }

  light->clear = (uint16_t) (((uint16_t) bytes[1] << 8U) | bytes[0]);
  light->red = (uint16_t) (((uint16_t) bytes[3] << 8U) | bytes[2]);
  light->green = (uint16_t) (((uint16_t) bytes[5] << 8U) | bytes[4]);
  light->blue = (uint16_t) (((uint16_t) bytes[7] << 8U) | bytes[6]);

  return HAL_OK;
}

HAL_StatusTypeDef Apds9960_ReadProximity(I2C_HandleTypeDef *hi2c,
                                         uint8_t *proximity)
{
  uint8_t status;
  HAL_StatusTypeDef result;

  if ((hi2c == NULL) || (proximity == NULL))
  {
    return HAL_ERROR;
  }

  result = Apds9960_ReadRegister(hi2c, APDS9960_REGISTER_STATUS, &status);
  if (result != HAL_OK)
  {
    return result;
  }

  if ((status & APDS9960_STATUS_PVALID) == 0U)
  {
    return HAL_BUSY;
  }

  return Apds9960_ReadRegister(hi2c, APDS9960_REGISTER_PDATA, proximity);
}

HAL_StatusTypeDef Apds9960_InitGesture(I2C_HandleTypeDef *hi2c)
{
  HAL_StatusTypeDef status;

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_ENABLE, 0U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_WTIME, 0xFFU);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_PPULSE, 0x87U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_CONFIG2, 0x01U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_GPENTH, 40U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_GEXTH, 30U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_GCONF1, 0x40U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_GCONF2, 0x41U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_GPULSE, 0xC9U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_GCONF3, 0U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_GCONF4, 0U);
  if (status != HAL_OK)
  {
    return status;
  }

  status = Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_ENABLE,
                                  APDS9960_ENABLE_PON);
  if (status != HAL_OK)
  {
    return status;
  }

  HAL_Delay(10U);

  return Apds9960_WriteRegister(hi2c, APDS9960_REGISTER_ENABLE,
                                APDS9960_ENABLE_PON | APDS9960_ENABLE_AEN |
                                    APDS9960_ENABLE_PEN |
                                    APDS9960_ENABLE_WEN |
                                    APDS9960_ENABLE_GEN);
}

HAL_StatusTypeDef Apds9960_ReadGesture(I2C_HandleTypeDef *hi2c,
                                       Apds9960Gesture *gesture)
{
  uint8_t gesture_status;
  uint8_t fifo_level;
  uint8_t fifo_bytes[APDS9960_GESTURE_FIFO_MAX_DATASET * 4U];
  uint8_t first_u = 0U;
  uint8_t first_d = 0U;
  uint8_t first_l = 0U;
  uint8_t first_r = 0U;
  uint8_t last_u = 0U;
  uint8_t last_d = 0U;
  uint8_t last_l = 0U;
  uint8_t last_r = 0U;
  bool first_sample_found = false;
  HAL_StatusTypeDef status;

  if ((hi2c == NULL) || (gesture == NULL))
  {
    return HAL_ERROR;
  }

  *gesture = APDS9960_GESTURE_NONE;

  status = Apds9960_ReadRegister(hi2c, APDS9960_REGISTER_GSTATUS,
                                 &gesture_status);
  if (status != HAL_OK)
  {
    return status;
  }

  if ((gesture_status & APDS9960_GSTATUS_GVALID) == 0U)
  {
    return HAL_BUSY;
  }

  status = Apds9960_ReadRegister(hi2c, APDS9960_REGISTER_GFLVL, &fifo_level);
  if (status != HAL_OK)
  {
    return status;
  }

  if (fifo_level == 0U)
  {
    return HAL_BUSY;
  }

  if (fifo_level > APDS9960_GESTURE_FIFO_MAX_DATASET)
  {
    fifo_level = APDS9960_GESTURE_FIFO_MAX_DATASET;
  }

  status = HAL_I2C_Mem_Read(hi2c, APDS9960_I2C_ADDRESS,
                            APDS9960_REGISTER_GFIFO_U, I2C_MEMADD_SIZE_8BIT,
                            fifo_bytes, (uint16_t) (fifo_level * 4U),
                            APDS9960_I2C_TIMEOUT_MS);
  if (status != HAL_OK)
  {
    return status;
  }

  for (uint8_t index = 0U; index < fifo_level; index++)
  {
    uint8_t u = fifo_bytes[(index * 4U) + 0U];
    uint8_t d = fifo_bytes[(index * 4U) + 1U];
    uint8_t l = fifo_bytes[(index * 4U) + 2U];
    uint8_t r = fifo_bytes[(index * 4U) + 3U];
    uint16_t total = (uint16_t) u + d + l + r;

    if (total < APDS9960_GESTURE_MIN_TOTAL)
    {
      continue;
    }

    if (!first_sample_found)
    {
      first_sample_found = true;
      first_u = u;
      first_d = d;
      first_l = l;
      first_r = r;
    }

    last_u = u;
    last_d = d;
    last_l = l;
    last_r = r;
  }

  if (!first_sample_found)
  {
    return HAL_BUSY;
  }

  *gesture = DecodeGesture(first_u, first_d, first_l, first_r, last_u, last_d,
                           last_l, last_r);

  return (*gesture == APDS9960_GESTURE_NONE) ? HAL_BUSY : HAL_OK;
}
