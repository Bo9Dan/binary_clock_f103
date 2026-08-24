#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include <stdbool.h>

void AppScheduler_On10msTickFromIsr(void);
bool AppScheduler_Consume10msTick(void);
bool AppScheduler_Consume500msTick(void);
bool AppScheduler_Consume1sTick(void);

#endif
