/*
 * Blink.cpp
 *
 * Created: 12.06.2016 19:00:37
 * Author : Alexey N. Zhukov
 */ 

#include "arduino.h"

void setup(void) {
	pinMode(4, OUTPUT);
}

void loop(void) {
	digitalWrite(4, HIGH);
	delay(100);
	digitalWrite(4, LOW);
	delay(100);
}
