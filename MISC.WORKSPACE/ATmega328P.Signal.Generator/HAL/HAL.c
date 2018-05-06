#include <HAL.h>

inline void InitAll(void) {
	// InitUSART
	UBRR0L = LO(bauddivider);
	UBRR0H = HI(bauddivider);
	UCSR0A = 0;
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0) | (0 << TXCIE0) | (0 << UDRIE0);
	UCSR0C = (1 << UCSZ00) | (1 << UCSZ01);

	// InitPort
	PORTD = 0;
	DDRD |= (1 << PORTD4);

	//Init ADC

	//Init PWM

	//Init Interrupts

	//Init Timers
}
