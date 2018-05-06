/*
 * uartutils.h
 *
 * Created: 17.09.2017 9:17:22
 *  Author: Alexey N. Zhukov
 */ 


#ifndef UARTUTILS_H_
#define UARTUTILS_H_

#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint8_t g_intUARTRxBuffer[UART_RX_BUFFER_SIZE];
extern volatile uint8_t g_intUARTRxIndex;

typedef void (*UART_RX)(uint8_t theData);
typedef void (*UART_TX)();

extern UART_RX onRx;
extern UART_TX onTx;

bool sendByte(uint8_t theValue);
bool sendString(uint8_t *theValue, uint8_t theSize, bool doCRLF);

#ifdef __cplusplus
}
#endif

#endif /* UARTUTILS_H_ */