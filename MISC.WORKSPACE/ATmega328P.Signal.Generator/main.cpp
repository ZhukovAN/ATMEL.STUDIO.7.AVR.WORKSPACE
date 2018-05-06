/*
 * ATmega328P.Signal.Generator.cpp
 *
 * Created: 12.09.2017 16:46:29
 * Author : Alexey N. Zhukov
 */ 

#include <avr/io.h>
#include "HAL.h"
#include "EERTOS.h"
#include "timerutils.h"
#include "uartutils.h"
#include "UART.h"
#include "convert.h"
#include "SPI.h"

int main(void) {
	InitAll();
	InitRTOS();
	RunRTOS();
    onRx = &onRxImpl;

	while(1) {
		// Главный цикл диспетчера
		wdt_reset();	// Сброс собачьего таймера
		TaskManager();	// Вызов диспетчера
	}
}

