/*/
 * UART.c
 *
 * Created: 13.09.2017 11:22:05
 *  Author: Alexey N. Zhukov
 */ 

#include <avr/interrupt.h>
#include <stdbool.h>
#include <string.h>
#include "UART.h"
#include "EERTOS.h"
#include "timerutils.h"
#include "uartutils.h"
#include "convert.h"

#define CR 0x0D
#define LF 0x0A
#define UART_OUT_BUF_SIZE 16

void blinkOn();
void blinkOff();

void sendRealFreq();
void processUART();

inline void onRxImpl(uint8_t theValue) {
    // Работаем через очередь задач, поэтому на каждую команду создается отдельная задача
    // Это позволяет нам обрабатывать в processUART команды поодиночке не заморачиваясь на
    // то, что в процессе чтения в буфер попадет еще одна команда
    if ((CR == theValue) || (LF == theValue)) SetTask(processUART);
}

char g_strOutBuffer[UART_OUT_BUF_SIZE];

void onTxImpl() {
    SetTask(sendRealFreq);
}

float g_fltResultFrequency = 0;

void sendRealFreq() {
    g_strOutBuffer[0] = '<';
    my_ftoa(g_fltResultFrequency, &g_strOutBuffer[1]);
    onTx = NULL;
    sendString((uint8_t*)(g_strOutBuffer), strlen(g_strOutBuffer), true);
}

volatile uint8_t g_intUARTRxReadIndex = 0;

uint8_t g_intOutBuffer[UART_OUT_BUF_SIZE];

void processUART() {
	float l_fltTempFrequency = 0;
	uint8_t l_intValue = 0;
	float l_fltValues[2] = {0, 0};
	uint8_t l_intIdx = 0;
	bool l_boolError = false;
	bool l_boolOK = false;
	bool l_boolEOB = false;
	do {
		l_intValue = g_intUARTRxBuffer[g_intUARTRxReadIndex++];
		if (g_intUARTRxReadIndex >= UART_RX_BUFFER_SIZE) g_intUARTRxReadIndex = 0;
		// Дошли до конца буфера
		l_boolEOB = (g_intUARTRxReadIndex == g_intUARTRxIndex);
		if (':' == l_intValue) {
			l_intIdx++;
			if (1 < l_intIdx) l_boolError = true;
		} else if (('0' <= l_intValue) && ('9' >= l_intValue)) {
			l_fltValues[l_intIdx] = l_fltValues[l_intIdx] * 10 + l_intValue - '0';
		} else if ((CR == l_intValue) || (LF == l_intValue)) {
			l_boolOK = true;
		} else
			l_boolError = true;
	} while (!l_boolError && !l_boolOK && !l_boolEOB);
	if (!l_boolOK) return;
	if (0 == l_intIdx)
		l_fltTempFrequency = l_fltValues[0];
	else if (0 != l_fltValues[1])
		l_fltTempFrequency = l_fltValues[0]/l_fltValues[1];
	else
		return;
	uint16_t l_intOCR;
	uint16_t l_intPrescaler;
	if (!bestTimer(l_fltTempFrequency, &l_intOCR, &l_intPrescaler, &g_fltResultFrequency)) return;
	initTimer(l_intOCR, l_intPrescaler);
    g_strOutBuffer[0] = '>';
    my_ftoa(l_fltTempFrequency, &g_strOutBuffer[1]);
    //my_ftoa(l_fltResultFrequency, g_strOutBuffer[1]);
    onTx = &onTxImpl;
    sendString((uint8_t*)(g_strOutBuffer), strlen(g_strOutBuffer), true);
}

void setFrequency() {
	
}

void blinkOn() {
	PORTD ^= (1 << PORTD4);
	SetTimerTask(blinkOff, 500);
}

void blinkOff() {
	PORTD &= ~(1 << PORTD4);
}
