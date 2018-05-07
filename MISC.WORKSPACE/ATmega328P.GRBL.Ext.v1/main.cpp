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
#include "HD44780.v1.h"
#include "FrequencyMeasure.h"
#include "fastmath.h"
#include "uartutils.h"
// #include "UART.h"
#include <string.h>
#include <util/delay.h>
#include "convert.h"
#include "DebugTools.h"
#ifdef __cplusplus
}
#endif

void SendRealFreq();

HD44780 l_objLCD1(0x27, 20, 4);
HD44780 l_objLCD2(0x20, 20, 4);

#define _DEBUG_FREQ_BUF_SIZE 16
char _g_strFreqOutBuffer[_DEBUG_FREQ_BUF_SIZE];

int main() {
    InitAll();
	_delay_ms(10);
	
	InitRTOS();
	RunRTOS();

 	l_objLCD1.begin();
    l_objLCD1.backlight(true);	 
 	l_objLCD2.begin();
 	l_objLCD2.backlight(true);
    SetTask(FreqTimerInit);
    SetTask(Capture);
	SetTask(SendRealFreq);
    // onRx = &onRxImpl;

	while (1) {
		// Главный цикл диспетчера
		wdt_reset();    // Сброс собачьего таймера
		TaskManager(); // Вызов диспетчера
	}
	return 0;
}

#define UART_OUT_BUF_SIZE 16
char g_strOutBuffer[UART_OUT_BUF_SIZE];

void SendRealFreq() {
	my_ftoa(GetFrequency(), &g_strOutBuffer[0]);
	onTx = NULL;
	SendString((uint8_t*)(g_strOutBuffer), strlen(g_strOutBuffer), true);
    l_objLCD1.printAt(0, 1, g_strOutBuffer, strlen(g_strOutBuffer));
    l_objLCD2.printAt(0, 0, g_strOutBuffer, strlen(g_strOutBuffer));
	SetTimerTask(SendRealFreq, 1000);
}
