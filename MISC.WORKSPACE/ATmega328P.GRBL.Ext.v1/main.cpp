/*
 * Blink.cpp
 *
 * Created: 12.06.2016 19:00:37
 * Author : Alexey N. Zhukov
 */ 

#ifdef __cplusplus
extern "C" {
#endif
#include "HAL.h"
#include "EERTOS.h"
/*
#include "IIC_ultimate.h"
*/
/*
#include "HD44780.v1.h"
#include "FreqTimer.h"
#include "fastmath.h"
#include "uartutils.h"
#include <string.h>
#include <util/delay.h>
#include "convert.h"
*/
#ifdef __cplusplus
}
#endif

// void SendRealFreq();

void d7off() {
	PORTD &= ~(1 << PORTD7);
}

void d7on() {
	DDRD |= (1 << PORTD7);
	PORTD |= (1 << PORTD7);
	SetTimerTask(d7off, 100);
};

void d6on();
void d6off() {
	PORTD &= ~(1 << PORTD6);
	SetTimerTask(d6on, 10);
}

void d6on() {
	DDRD |= (1 << PORTD6);
	PORTD |= (1 << PORTD6);
	SetTimerTask(d6off, 10);
};



// #include "HD44780.v1.h"


// uint8_t UART_RX;
// Адрес PCF8574 с A2:0 = 000 - 64 (0x40)
// HD44780 l_objLCD(0x27, 20, 4);

int main() {
    //InitAll();
	// _delay_ms(1000);
	// begin(0x40, 20, 4);
	
	InitRTOS();
	RunRTOS();

 	// l_objLCD.begin();
 	// l_objLCD.printAt(0, 0, (char*)("01234567890123456789"), 20);

    // SetTask(FreqTimerInit);
    // SetTask(Capture);
	// SetTask(SendRealFreq);
	SetTask(d6on);
	SetTask(d7on);
	while (1) {
		// Главный цикл диспетчера
		wdt_reset();    // Сброс собачьего таймера
		TaskManager(); // Вызов диспетчера
	}
	return 0;
}
/*
#define UART_OUT_BUF_SIZE 16
char g_strOutBuffer[UART_OUT_BUF_SIZE];

void SendRealFreq() {
	my_ftoa(GetFrequency(), &g_strOutBuffer[0]);
	onTx = NULL;
	SendString((uint8_t*)(g_strOutBuffer), strlen(g_strOutBuffer), true);
    // l_objLCD.printAt(0, 1, g_strOutBuffer, strlen(g_strOutBuffer));
	SetTimerTask(SendRealFreq, 1000);
}
*/