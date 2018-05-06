/*
 * HD44780.c
 *
 * Created: 24.08.2017 17:12:53
 *  Author: Alexey N. Zhukov
 */ 

#include "HD44780.h"
#include "IIC_ultimate.h"
#include <util/delay.h>
#include <binary.h>
#include <stdlib.h>
#include "fastmath.h"

#define En B00000100  // Enable bit
#define Rw B00000010  // Read/Write bit
#define Rs B00000001  // Register select bit

uint8_t g_intAddr = 0x40;
uint8_t g_intCols = 20;
uint8_t g_intRows = 4;
uint8_t g_intDisplayFunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
uint8_t g_intDisplayControl = 0;
uint8_t g_intDisplayMode = 0;
uint8_t g_intCharSize = LCD_5x8DOTS;

volatile bool writeComplete = false;

bool writeAsync(uint8_t theAddress, uint8_t theData);
bool writeBufAsync(uint8_t theAddress, uint8_t* theData, uint8_t theSize);
bool writeBuf(uint8_t theAddress, uint8_t* theData, uint8_t theSize);

inline bool write(uint8_t theAddress, uint8_t theData) {
	return writeBuf(theAddress, &theData, 1);
}

bool writeBuf(uint8_t theAddress, uint8_t* theData, uint8_t theSize) {
	if (!writeBufAsync(theAddress, theData, theSize)) return false;
	while (!writeComplete);
	return !(i2c_Do & (i2c_ERR_NA | i2c_ERR_BF));
}

void writeAsyncComplete() {
	i2c_Do &= i2c_Free;											// Освобождаем шину
	writeComplete = true;
}

bool writeBufAsync(uint8_t theAddress, uint8_t* theData, uint8_t theSize) {
	if (i2c_Do & i2c_Busy)
	// Если передатчик занят - выходим
	return false;

	i2c_index = 0;
	i2c_ByteCount = theSize;

	i2c_SlaveAddress = theAddress;
	if (NULL == GetDataFunc) {
		// Если данные передаются не через callback-функцию, то придется копировать их во внутренний буфер I2C
		uint8_t l_intSize = i2c_MaxBuffer < theSize ? i2c_MaxBuffer : theSize;
			for (uint8_t i = 0 ; i < l_intSize ; i++)
				i2c_Buffer[i] = *theData++;
	}
	i2c_Do = i2c_sawp;							// Режим = простая запись

	MasterOutFunc = &writeAsyncComplete;		// Точка выхода из автомата если все хорошо
	ErrorOutFunc = &writeAsyncComplete;			// И если все плохо.
	
	writeComplete = false;

	TWCR = 1<<TWSTA|0<<TWSTO|1<<TWINT|0<<TWEA|1<<TWEN|1<<TWIE;

	i2c_Do |= i2c_Busy;							// Шина занята!
	
	return true;
}

inline bool writeAsync(uint8_t theAddress, uint8_t theData) {
	return writeBufAsync(theAddress, &theData, 1);
}

void write4bits(uint8_t theAddress, uint8_t theData) {
	write(theAddress, theData & ~En);	// En low
	write(theAddress, theData | En);	// En high
	// _delay_us(1);		// enable pulse must be >450ns
	write(theAddress, theData & ~En);	// En low
	// _delay_us(50);
}

void send(uint8_t theAddress, uint8_t theData, uint8_t theMode) {
	uint8_t l_intHigh = theData & 0xF0;
	uint8_t l_intLow = (theData << 4) & 0xF0;
	write4bits(theAddress, l_intHigh | theMode);
	write4bits(theAddress, l_intLow | theMode);
}

void sendBuf(uint8_t theAddress, uint8_t* theData, uint8_t theSize, uint8_t theMode) {
	uint8_t l_intSize = theSize * 6;
	uint8_t *l_ptrData = malloc(l_intSize);
	uint8_t *l_ptrIdx = l_ptrData; 
	for (uint8_t i = 0; i < theSize; i++) {
		uint8_t l_intValue = *theData++;
		for (uint8_t j = 0 ; j < 2 ; j++) {
			uint8_t l_int4Bit = (0 == j) ? l_intValue & 0xF0 : (l_intValue << 4) & 0xF0;
			l_int4Bit |= theMode;
			*l_ptrIdx = l_int4Bit & ~En;
			l_ptrIdx++;
			*l_ptrIdx = l_int4Bit | En;
			l_ptrIdx++;
			*l_ptrIdx = l_int4Bit & ~En;
			l_ptrIdx++;
		}
	}
	writeBuf(theAddress, l_ptrData, l_intSize);
	free(l_ptrData);
}

uint8_t getData(uint16_t theIdx) {
	uint16_t l_intIdx = div3u16(theIdx) >> 1;
	uint8_t l_intStep = theIdx - l_intIdx * 6;
	uint8_t l_intData = hd44780_Buffer[l_intIdx];
	uint8_t l_int4Bit = (3 > l_intStep) ? l_intData & 0xF0 : (l_intData << 4) & 0xF0;
	l_int4Bit |= Rs;
	
	switch (l_intStep) {
		case 1 :
		case 4 : return l_int4Bit | En;
		default : return l_int4Bit & ~En;
	}
}

void sendBufExt(uint8_t theAddress, uint8_t* theData, uint8_t theSize, uint8_t theMode) {
	uint8_t l_intSize = theSize > hd44780_MaxBuffer ? hd44780_MaxBuffer : theSize;
	for (uint8_t i = 0 ; i < l_intSize ; i++)
		hd44780_Buffer[i] = *theData++;
	GetDataFunc = &getData;
	writeBuf(theAddress, NULL, theSize * 6);
	GetDataFunc = NULL;
}

void command(uint8_t theAddress, uint8_t theData) {
	send(theAddress, theData, 0);
}

void display() {
	g_intDisplayControl |= LCD_DISPLAYON;
	command(g_intAddr, LCD_DISPLAYCONTROL | g_intDisplayControl);
}

void clear() {
	command(g_intAddr, LCD_CLEARDISPLAY);	// clear display, set cursor position to zero
	_delay_us(2000);			// this command takes a long time!
}

void home() {
	command(g_intAddr, LCD_RETURNHOME);	// set cursor position to zero
	_delay_us(2000);			// this command takes a long time!
}

void setCursor(uint8_t theCol, uint8_t theRow){
	int l_intRowOffsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if (theRow > g_intRows)
		theRow = g_intRows - 1;    // we count rows starting w/0
	command(g_intAddr, LCD_SETDDRAMADDR | (theCol + l_intRowOffsets[theRow]));
}


void begin(uint8_t theAddress, uint8_t theCols, uint8_t theRows) {
	g_intAddr = theAddress;
	g_intCols = theCols;
	g_intRows = theRows;
	
	g_intDisplayFunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
	if (g_intRows > 1) 
		g_intDisplayFunction |= LCD_2LINE;

	// for some 1 line displays you can select a 10 pixel high font
	if ((0 != g_intCharSize) && (1 == g_intRows)) 
		g_intDisplayFunction |= LCD_5x10DOTS;

	write4bits(g_intAddr, 0x03 << 4);
	_delay_us(4500);
	write4bits(g_intAddr, 0x03 << 4);
	_delay_us(150);
	write4bits(g_intAddr, 0x03 << 4);
	write4bits(g_intAddr, 0x02 << 4);

	command(g_intAddr, LCD_FUNCTIONSET | g_intDisplayFunction);	
	
	// turn the display on with no cursor or blinking default
	g_intDisplayControl = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	display();
	
	// clear it off
	clear();
	
	// Initialize to default text direction (for roman languages)
	g_intDisplayMode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	
	// set the entry mode
	command(g_intAddr, LCD_ENTRYMODESET | g_intDisplayMode);
	
	home();
	setCursor(1, 1);
	send(g_intAddr, (uint8_t)'1', Rs);
	// send(g_intAddr, 0x41, Rs);
	/*send(g_intAddr, 0x42, Rs);
	sendBufExt(g_intAddr, (uint8_t*)"01234567890123456789", 20, Rs);*/
	// sendBuf(g_intAddr, (uint8_t*)"CD", 2, Rs);
	// setCursor(0, 3);
	// sendBufExt(g_intAddr, (uint8_t*)"01234567890123456789", 20, Rs);
}



