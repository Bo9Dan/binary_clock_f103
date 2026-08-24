#include "app_scheduler.h"
#include "stm32f1xx_hal.h"

#define TICKS_10MS_PER_500MS 50U
#define TICKS_10MS_PER_1S 100U

static volatile uint8_t tick_10ms_pending;
static volatile uint8_t tick_500ms_pending;
static volatile uint8_t tick_1s_pending;

static uint8_t tick_500ms_accumulator;
static uint8_t tick_1s_accumulator;

static void IncrementPending(volatile uint8_t *counter)
{
  if (*counter < UINT8_MAX)
  {
    (*counter)++;
  }
}

static bool ConsumePending(volatile uint8_t *counter)
{
  bool pending;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  pending = *counter > 0U;
  if (pending)
  {
    (*counter)--;
  }
  if (primask == 0U)
  {
    __enable_irq();
  }

  return pending;
}

void AppScheduler_On10msTickFromIsr(void)
{
  IncrementPending(&tick_10ms_pending);

  tick_500ms_accumulator++;
  if (tick_500ms_accumulator >= TICKS_10MS_PER_500MS)
  {
    tick_500ms_accumulator = 0U;
    IncrementPending(&tick_500ms_pending);
  }

  tick_1s_accumulator++;
  if (tick_1s_accumulator >= TICKS_10MS_PER_1S)
  {
    tick_1s_accumulator = 0U;
    IncrementPending(&tick_1s_pending);
  }
}

bool AppScheduler_Consume10msTick(void)
{
  return ConsumePending(&tick_10ms_pending);
}

bool AppScheduler_Consume500msTick(void)
{
  return ConsumePending(&tick_500ms_pending);
}

bool AppScheduler_Consume1sTick(void)
{
  return ConsumePending(&tick_1s_pending);
}
