/*
 * Blink.cpp
 *
 * Created: 12.06.2016 19:00:37
 * Author : Alexey N. Zhukov
 */ 

#include "arduino.h"
#include <Ethernet.h>

EthernetServer server = EthernetServer(80);

void setup(void) {
	pinMode(13, OUTPUT);
}

void loop(void) {
	digitalWrite(13, HIGH);
	delay(500);
	digitalWrite(13, LOW);
	delay(500);
}
