/*
 * Blink.cpp
 *
 * Created: 12.06.2016 19:00:37
 * Author : Alexey N. Zhukov
 */ 

#include <arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 20 chars and 4 line display
LiquidCrystal_I2C lcd(0x27, 20, 4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

volatile unsigned long g_intCounter = 0;
unsigned long g_intPrevCounter = 0;
void count() {
	g_intCounter++;
}

void setup(void) {
	lcd.init();                      // initialize the lcd
	// Print a message to the LCD.
	lcd.backlight();
    attachInterrupt(0, count, RISING);  //attaching the interrupt and declaring the variables, one of the interrupt pins on Nano is D2, and has to be declared as 0 here
}

unsigned long timeold = 0;

void loop(void) {
	if (0 == g_intCounter) return;
	detachInterrupt(0);

	float rpm = 60000.0/(millis()-timeold)*g_intCounter;
	rpm /= 2;
	float rps=rpm/60.0;
	timeold = millis();
	g_intCounter=0;

	lcd.setCursor (0,1);
	lcd.print(rps);
	lcd.print("RPS ");
	lcd.setCursor(8, 1);
	attachInterrupt(0, count, RISING);
	delay(1000);
}
