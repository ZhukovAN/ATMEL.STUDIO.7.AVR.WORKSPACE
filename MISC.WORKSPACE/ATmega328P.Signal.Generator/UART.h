/*
 * UART.h
 *
 * Created: 13.09.2017 11:22:58
 *  Author: Alexey N. Zhukov
 */ 


#ifndef UART_H_
#define UART_H_

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void onRxImpl(uint8_t theValue);
extern void onTxImpl();

#ifdef __cplusplus
}
#endif


#endif /* UART_H_ */