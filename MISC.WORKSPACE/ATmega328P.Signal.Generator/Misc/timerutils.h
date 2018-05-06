/*
 * timerutils.h
 *
 * Created: 13.09.2017 10:21:53
 *  Author: Alexey N. Zhukov
 */ 


#ifndef TIMERUTILS_H_
#define TIMERUTILS_H_

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <avr/io.h>

#ifdef __cplusplus
extern "C" {
#endif

// Функция вычисляет оптимальное (с точки зрения погрешности выходной частоты по 
// отношению к желаемой) значение регистра сравнения 16-битного таймера и коэффициента 
// деления для работы в режиме CTC
bool bestTimer(float theFrequency, uint16_t* theOCR, uint16_t* thePrescaler, float* theResultFrequency);

void initTimer(uint16_t theOCR, uint16_t thePrescaler);

#ifdef __cplusplus
}
#endif

#endif /* TIMERUTILS_H_ */