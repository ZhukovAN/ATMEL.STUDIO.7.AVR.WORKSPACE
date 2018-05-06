/*
 * uartutils.c
 *
 * Created: 17.09.2017 9:17:35
 *  Author: Alexey N. Zhukov
 */ 

#include <avr/interrupt.h>
#include <stddef.h>
#include "uartutils.h"

volatile uint8_t g_intUARTRxBuffer[UART_RX_BUFFER_SIZE];
volatile uint8_t g_intUARTRxIndex;

volatile uint8_t g_intUARTTxBuffer[UART_TX_BUFFER_SIZE];
volatile uint8_t g_intUARTTxIndex;
volatile uint8_t g_intUARTTxByteCount;
volatile bool g_boolTxBusy;

UART_RX onRx = NULL;
UART_TX onTx = NULL;

ISR(USART_RX_vect) {
    uint8_t l_intValue = UDR0;
    g_intUARTRxBuffer[g_intUARTRxIndex] = l_intValue;
    g_intUARTRxIndex++;
    if (g_intUARTRxIndex >= UART_RX_BUFFER_SIZE) g_intUARTRxIndex = 0;
    if (NULL != onRx) onRx(l_intValue);
}

ISR(USART_UDRE_vect) {
    // Увеличиваем индекс
    g_intUARTTxIndex++;
    
    if ((g_intUARTTxByteCount == g_intUARTTxIndex) || (UART_TX_BUFFER_SIZE == g_intUARTTxIndex)) {
        // Вывели весь буффер?
        // Запрещаем прерывание по опустошению - передача закончена
        UCSR0B &= ~(1 << UDRIE0);	
        g_boolTxBusy = false;
        if (NULL != onTx) onTx();
    } else
        // Берем данные из буффера
        UDR0 = g_intUARTTxBuffer[g_intUARTTxIndex];	
}

bool sendByte(uint8_t theValue) {
    if (g_boolTxBusy) return false;
    g_boolTxBusy = true;
    // Сбрасываем индекс
    g_intUARTTxIndex = 0;		
    g_intUARTTxBuffer[0] = theValue;
    g_intUARTTxByteCount = 1;
    // Отправляем первый байт
    UDR0 = g_intUARTTxBuffer[0];		
    UCSR0B |= (1 << UDRIE0);
    return true;
}

bool sendString(uint8_t *theValue, uint8_t theSize, bool doCRLF) {
    if (g_boolTxBusy) return false;
    g_boolTxBusy = true;
    g_intUARTTxByteCount = (theSize > UART_TX_BUFFER_SIZE) ? UART_TX_BUFFER_SIZE : theSize;
    for (uint8_t i = 0 ; i < g_intUARTTxByteCount ; i++)
        g_intUARTTxBuffer[i] = *theValue++;
    if (doCRLF) {
        g_intUARTTxBuffer[g_intUARTTxByteCount++] = '\r';
        g_intUARTTxBuffer[g_intUARTTxByteCount++] = '\n';
    }
    // Сбрасываем индекс
    g_intUARTTxIndex = 0;
    // Отправляем первый байт, остальные - через обработчик прерывания
    UDR0 = g_intUARTTxBuffer[0];
    UCSR0B |= (1 << UDRIE0);
    return true;
}