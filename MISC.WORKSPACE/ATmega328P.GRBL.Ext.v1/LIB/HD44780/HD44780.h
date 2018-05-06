/*
 * HD44780.h
 *
 * Created: 24.08.2017 17:13:30
 *  Author: Alexey N. Zhukov
 */ 


#ifndef HD44780_H_
#define HD44780_H_

#include <stdbool.h>
#include <inttypes.h>

// ����� ��� �������� ������. ��� ������ 20x4 ����� ��������� ������, ��� ����� 
// ������ ������� � ������ ������ � ��������� ������� ��������� ����� ������� �� 
// �� ������, � � ������� ������. �� ���� ����������� ����������� ����� ������ 
// ���������� 20 ��������, ������ ������� ����� �������� ��� ����� �������� �������� 
// �� ������. ����� �������, ����������� ����������� ������ ������ ���������� 
// 21 ���� (���� ���� �� ������� � 20 - �� ������)
#define hd44780_MaxBuffer 21

uint8_t hd44780_Buffer[hd44780_MaxBuffer];			// ����� ��� ������ ������ � ������ Master

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

bool write(uint8_t theAddress, uint8_t theData);
void begin(uint8_t theAddress, uint8_t theCols, uint8_t theRows);
bool writeBufAsync(uint8_t theAddress, uint8_t* theData, uint8_t theSize);
void sendBuf(uint8_t theAddress, uint8_t* theData, uint8_t theSize, uint8_t theMode);
void sendBufExt(uint8_t theAddress, uint8_t* theData, uint8_t theSize, uint8_t theMode);

#endif /* HD44780_H_ */