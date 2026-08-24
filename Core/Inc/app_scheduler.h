#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

void AppScheduler_On10msTickFromIsr(void);
uint32_t AppScheduler_NowMs(void);
bool AppScheduler_Consume10msTick(void);
bool AppScheduler_Consume120msTick(void);
bool AppScheduler_Consume500msTick(void);
bool AppScheduler_Consume1sTick(void);
bool AppScheduler_Consume60sTick(void);

#endif
