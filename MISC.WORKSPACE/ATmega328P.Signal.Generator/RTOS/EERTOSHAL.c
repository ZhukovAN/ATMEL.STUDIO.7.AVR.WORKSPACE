#include <avr/interrupt.h>
#include "EERTOSHAL.h"

//RTOS ������ ���������� �������
inline void RunRTOS (void) {
#ifdef __AVR_ATmega328P__
	// TCCR2A:: WGM22 = 0, WGM21 = 1, WGM20 = 0 - CTC mode
	// TCCR2B:: CS22 = 1, CS21 = 0, CS20 = 0 - Prescaler 64
	TCCR2A = 1 << WGM21;
	TCCR2B = 1 << CS22;
	TCNT2 = 0; 
	OCR2A = TimerDivider & 0xFF;
	TIMSK2 |= 1 << OCIE2A;
#elif defined (__AVR_ATmega8__)
	// TCCR2:: WGM21 = 1, WGM20 = 0 - CTC mode
	// TCCR2:: CS22 = 1, CS21 = 0, CS20 = 0 - Prescaler 64
	TCCR2 = 1<<WGM21|1<<CS22; 				// Freq = CK/64 - ���������� ����� � ������������
	// ��������� ����� ���������� �������� ���������
	TCNT2 = 0;								// ���������� ��������� �������� ���������
	OCR2  = LO(TimerDivider); 				// ���������� �������� � ������� ���������
	TIMSK |= 1<<OCIE2;						// ��������� ���������� RTOS - ������ ��
#endif
	sei();
}
