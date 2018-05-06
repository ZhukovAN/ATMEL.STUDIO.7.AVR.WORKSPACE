#ifndef EERTOSHAL_H
#define EERTOSHAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <avr/io.h>
#include <avr/interrupt.h>

//System Timer Config
#define Prescaler	  		64
#define	TimerDivider  		(F_CPU/Prescaler/1000)		// 1 mS

#define STATUS_REG 			SREG
#define Interrupt_Flag		SREG_I
#define Disable_Interrupt	cli();
#define Enable_Interrupt	sei();

//RTOS Config
#ifdef __AVR_ATmega328P__
#define RTOS_ISR  			TIMER2_COMPA_vect
#define USART_RXC_vect      USART_RX_vect
#elif defined (__AVR_ATmega8__)
#define RTOS_ISR  			TIMER2_COMP_vect
#endif

#define	TaskQueueSize		30
#define MainTimerQueueSize	30

extern void RunRTOS (void);

#ifdef __cplusplus
}
#endif


#endif
