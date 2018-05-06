/*
 * DebugTools.c
 *
 * Created: 19.09.2017 13:00:46
 *  Author: Alexey N. Zhukov
 */ 

#include <avr/io.h>
#include "EERTOS.h"
#include "DebugTools.h"

void d2on() {
    #ifdef DEBUG
    PORTD |= (1 << PORTD2); 
    SetTimerTask(d2off, 100);
    #endif
};

void d2off() {
    #ifdef DEBUG
    PORTD &= ~(1 << PORTD2); 
    #endif
}

void d3on() {
    #ifdef DEBUG
    PORTD |= (1 << PORTD3);
    SetTimerTask(d3off, 100);
    #endif
};

void d3off() {
    #ifdef DEBUG
    PORTD &= ~(1 << PORTD3);
    #endif
}

void d4on() {
    #ifdef DEBUG
    PORTD |= (1 << PORTD4);
    SetTimerTask(d4off, 100);
    #endif
};

void d4off() {
    #ifdef DEBUG
    PORTD &= ~(1 << PORTD4);
    #endif
}
