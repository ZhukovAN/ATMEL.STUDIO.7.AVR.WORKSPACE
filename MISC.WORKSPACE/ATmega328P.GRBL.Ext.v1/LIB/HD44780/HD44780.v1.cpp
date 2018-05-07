/*
 * HD44780.v1.cpp
 *
 * Created: 06.09.2017 18:31:43
 *  Author: Alexey N. Zhukov
 */ 

#include <util/delay.h>
#include <stdlib.h>

#include "HD44780.v1.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "fastmath.h"
#ifdef __cplusplus
}
#endif

#define BLBIT   3
#define ENBIT   2
#define RWBIT   1
#define RSBIT   0
#define En (1 << ENBIT) // Enable bit
#define Rw (1 << RWBIT) // Read/Write bit
#define Rs (1 << RSBIT) // Register select bit

// flags for backlight control
#define LCD_BACKLIGHT (1 << BLBIT)
#define LCD_NOBACKLIGHT 0x00


// Commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// Flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// Flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// Flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// Flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x8DOTS 0x00
#define LCD_5x10DOTS 0x04

#ifdef DEBUG
#include "EERTOS.h"
// TODO: добавить отладочные уведомления об ошибках (выход за пределы буфера и т.д.)
#endif

HD44780* HD44780::g_ptrActiveLCD = NULL;

uint8_t GetData(uint16_t theIdx) {
	if (NULL == HD44780::g_ptrActiveLCD) return 0xFF;
	return HD44780::g_ptrActiveLCD->getData(theIdx);
}

void OnSendComplete() {
	// Освобождаем шину
	i2c_Do &= i2c_Free;											
	if (NULL == HD44780::g_ptrActiveLCD) return;
	HD44780::g_ptrActiveLCD->onSendComplete();
}

HD44780::HD44780(uint8_t theAddr, uint8_t theCols, uint8_t theRows) {
	this->m_intDisplayControl = 0;
	this->m_intDisplayMode = 0;
	this->m_intDisplayFunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
	this->m_intCharSize = LCD_5x8DOTS;

	this->m_intAddr = theAddr;
	this->m_intCols = theCols;
	this->m_intRows = theRows;
	this->m_intBufferSize = 0;
	// Буфер для передачи данных. Для экрана 20x4 схема адресации такова, что после
	// вывода символа в первой строке и последнем столбце следующий будет выведен не
	// во второй, а в третьей строке. То есть максимально необходимая длина строки
	// составляет 20 символов, строки длиннее этого значения все равно придется выводить
	// по частям. Таким образом, максимально необходимый размер буфера составляет
	// 20 байт	
	this->m_intMaxBufferSize = theCols;
	this->m_ptrBuffer = (uint8_t*)malloc(this->m_intMaxBufferSize);
	this->m_intCmd = 0;
	
	this->m_ptrOnSendComplete = NULL;
	this->m_boolSendComplete = true;
}

bool HD44780::lockI2C() {
	// I2C занята
	if (i2c_Do & i2c_Busy) return false;
	if (!this->m_boolSendComplete) return false;
	// Шина и так зарезервирована нами
	if (this == HD44780::g_ptrActiveLCD) return true;
	// Шина зарезервирована кем-то еще
	if (NULL != HD44780::g_ptrActiveLCD) return false;
	// Шина не зарезервирована - регистрируем себя
	HD44780::g_ptrActiveLCD = this;
	GetDataFunc = &GetData;
	this->m_boolSendComplete = false;
	return true;
}

bool HD44780::command(uint8_t theCmd, bool the4BitOnlyMode) {
	this->m_bool4BitOnlyMode = the4BitOnlyMode;
	this->m_intCmd = theCmd;
	this->m_intBufferSize = 0;
	bool l_boolRes = this->send();	
	this->m_intCmd = 0;
	return l_boolRes;
}

bool HD44780::data(uint8_t theData, bool the4BitOnlyMode) {
	this->m_bool4BitOnlyMode = the4BitOnlyMode;
	this->m_intCmd = 0;
	this->m_intBufferSize = 1;
	*this->m_ptrBuffer = theData;
	return this->send();
}

bool HD44780::releaseI2C() {
	if (this != HD44780::g_ptrActiveLCD) return false;
	GetDataFunc = NULL;
	this->m_boolSendComplete = true;
	HD44780::g_ptrActiveLCD = NULL;
	return true;
}

uint8_t HD44780::getData(uint16_t theIdx) {
	uint16_t l_intIdx = div3u16(theIdx);
	if (!this->m_bool4BitOnlyMode) l_intIdx >>= 1;
	bool l_boolIsCmd = (0 != this->m_intCmd) && (0 == l_intIdx);
	uint16_t l_intTemp = l_intIdx * 3;
	if (!this->m_bool4BitOnlyMode) l_intTemp <<= 1;
	uint8_t l_intStep = theIdx - l_intTemp;
	uint8_t l_intData;
	if (l_boolIsCmd) 
		l_intData = this->m_intCmd;
	else {
		if (0 != this->m_intCmd)
		// Если передавали команду, то индекс надо уменьшить на единицу
			l_intIdx--;
		uint8_t *l_ptrData = this->m_ptrBuffer + l_intIdx;
		l_intData = *l_ptrData;
	}
	uint8_t l_int4Bit = (3 > l_intStep) ? l_intData & 0xF0 : (l_intData << 4) & 0xF0;
	if (!l_boolIsCmd)
		// При передаче команды бит Rs должен быть установлен в 0, данных - в 1
		l_int4Bit |= Rs;
	switch (l_intStep) {
		case 1 :
		case 4 : return l_int4Bit | En | this->m_intBackLight;
		default : return l_int4Bit & ~En | this->m_intBackLight;
	}
}

bool HD44780::send() {
	if (!this->send(NULL)) return false;
	while (!this->m_boolSendComplete);
	return !(i2c_Do & (i2c_ERR_NA | i2c_ERR_BF));
}

bool HD44780::send(IIC_F theOnSendComplete) {
	// Не удалось застолбить за собой I2C - выходим
	if (!this->lockI2C()) return false;
	this->m_ptrOnSendComplete = theOnSendComplete;
	i2c_index = 0;
	i2c_ByteCount = this->m_intBufferSize * 3;
	// Передаем команду?
	if (0 != this->m_intCmd) i2c_ByteCount += 3;
	if (!this->m_bool4BitOnlyMode) i2c_ByteCount <<= 1;

	i2c_SlaveAddress = this->m_intAddr;
	i2c_Do = i2c_sawp;							// Режим = простая запись

	MasterOutFunc = &OnSendComplete;			// Точка выхода из автомата если все хорошо
	ErrorOutFunc = &OnSendComplete;				// И если все плохо.
	
	TWCR = 1<<TWSTA|0<<TWSTO|1<<TWINT|0<<TWEA|1<<TWEN|1<<TWIE;
	i2c_Do |= i2c_Busy;							// Шина занята!
	return true;	
}

void HD44780::begin() {
	this->m_intDisplayFunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
	if (this->m_intRows > 1)
		this->m_intDisplayFunction |= LCD_2LINE;

	// for some 1 line displays you can select a 10 pixel high font
	if ((0 != this->m_intCharSize) && (1 == this->m_intRows))
		this->m_intDisplayFunction |= LCD_5x10DOTS;
	this->command(0x03 << 4, true);
	//_delay_us(4500);
	//this->command(0x03 << 4, true);
	_delay_us(4500);
	this->command(0x03 << 4, true);
	_delay_us(150);
	this->command(0x02 << 4, true);
	// Перешли в четырехбитный режим
	this->command(LCD_FUNCTIONSET | this->m_intDisplayFunction);
		
	// turn the display on with no cursor or blinking default
	this->m_intDisplayControl = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
	this->display();
	
	this->clear();
	// Initialize to default text direction (for roman languages)
	this->m_intDisplayMode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
	// Set the entry mode
	this->command(LCD_ENTRYMODESET | this->m_intDisplayMode);
	
	this->home();
}

bool HD44780::display() {
	this->m_intDisplayControl |= LCD_DISPLAYON;
	return this->command(LCD_DISPLAYCONTROL | this->m_intDisplayControl);
}

bool HD44780::clear() {
	// clear display, set cursor position to zero
	bool l_boolRes = this->command(LCD_CLEARDISPLAY);
	_delay_us(1520);
	return l_boolRes;
}

bool HD44780::home() {
	// set cursor position to zero
	bool l_boolRes = this->command(LCD_RETURNHOME);	
	// 1,52ms according to datasheet
	_delay_us(1520);
	return l_boolRes;
}

bool HD44780::printAt(uint8_t theCol, uint8_t theRow, char* theData, uint8_t theLength) {
	int l_intRowOffsets[] = { 0x00, 0x40, 0x14, 0x54 };
	if (theRow > this->m_intRows)
		theRow = this->m_intRows - 1;    // we count rows starting w/0
	this->m_intCmd = LCD_SETDDRAMADDR | (theCol + l_intRowOffsets[theRow]);
	this->m_intBufferSize = theLength;
	if (this->m_intBufferSize > this->m_intMaxBufferSize) this->m_intBufferSize = this->m_intMaxBufferSize;
	uint8_t *l_ptrBuffer = this->m_ptrBuffer;
	for (int i = 0 ; i < this->m_intBufferSize ; i++)
		*l_ptrBuffer++ = *theData++;
	return this->send();
}

void HD44780::onSendComplete() {
	this->releaseI2C();
	if (NULL != this->m_ptrOnSendComplete)
		this->m_ptrOnSendComplete();
}

void HD44780::backlight(bool theValue) {
	this->m_intBackLight = theValue ? LCD_BACKLIGHT : LCD_NOBACKLIGHT;
}

HD44780::~HD44780() {
	free(this->m_ptrBuffer);
}