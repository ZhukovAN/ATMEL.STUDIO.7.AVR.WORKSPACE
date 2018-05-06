#ifndef HAL_H
#define HAL_H

#ifdef __cplusplus
extern "C" {
#endif

//Clock Config
#ifndef F_CPU
	#define F_CPU 16000000L
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <stdlib.h>

#define INLINE __attribute__((always_inline))

//USART Config
#define baudrate 19200L
#define bauddivider (F_CPU / (baudrate * 16) - 1)
#define HI(x) ((x)>>8)
#define LO(x) ((x)& 0xFF)

//PORT Defines
#define LED0		3
#define LED0_PORT	PORTB
#define LED0_DDR	DDRB


#define LED1 		4
#define LED2		5
#define	LED3		6
#define	LED4		7
#define LED_PORT 	PORTD
#define LED_DDR		DDRD

#define SRV			3
#define SRV_P		PORTD
#define SRV_D		DDRD


#define SEND(X) do{buffer_index = 1; UDR = X; UCSRB|=(1<<UDRIE); }while(0)

extern void InitAll(void);

#ifdef __cplusplus
}
#endif

#endif
