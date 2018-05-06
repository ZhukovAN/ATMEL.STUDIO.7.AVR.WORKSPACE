/*
 * SPI.c
 *
 * Created: 15.09.2017 13:49:00
 *  Author: Alexey N. Zhukov
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include "SPI.h"

#ifdef __AVR_ATmega328P__
#define SS		PB2
#define MOSI	PB3
#define MISO	PB4
#define SCK		PB5
#endif 

#include "EERTOS.h"

void stop();
void _blinkOff();

void _blinkOn() {
    PORTD ^= (1 << PORTD4);
    SetTimerTask(_blinkOff, 500);
}

void _blinkOff() {
    PORTD &= ~(1 << PORTD4);
}

void start() {
    SetTask(Init_SPI);
}

void stop() {
    SPCR &= ~(1 << SPE);
    SPCR = 0;
}

void Init_SPI() {
	//Настроить выводы MOSI,SS,SCK на выход
	DDRB |= (1 << MOSI) | (1 << SS) | (1 << SCK); 
	//Настроить вывод MISO на вход
	DDRB &= ~(1 << MISO);
	//Установить "1" на линии SS
	PORTB |= (1 << SS); 
    // Режим мастер, F=Fosc/4
	SPCR = (1 << MSTR) | (0 << SPR1) | (0 << SPR0) | (1 << SPIE); 
    PORTB &= ~(1 << SS);
    // Режим мастер, F=Fosc/128
    // SPCR = (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
	//Включить SPI
	SPCR |= (1 << SPE);
    // SPDR = 0x0F;
    // while(!(SPSR & (1<<SPIF)));
    // PORTB |= (1 << SS);
    _blinkOn();
    SetTimerTask(stop, 1);
}
