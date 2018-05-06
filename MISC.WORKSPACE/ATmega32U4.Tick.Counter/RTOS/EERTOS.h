#ifndef EERTOS_H
#define EERTOS_H

#include "EERTOSHAL.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void InitRTOS(void);
extern void Idle(void);

typedef void (*TPTR)(void);

extern void SetTask(const TPTR TS);
extern void SetTimerTask(const TPTR TS, const uint16_t NewTime);

extern void TaskManager(void);
extern void TimerService(void);

//RTOS Errors Пока не используются.
#define TaskSetOk			 'A'
#define TaskQueueOverflow	 'B'
#define TimerUpdated		 'C'
#define TimerSetOk			 'D'
#define TimerOverflow		 'E'

#ifdef __cplusplus
}
#endif

#endif
