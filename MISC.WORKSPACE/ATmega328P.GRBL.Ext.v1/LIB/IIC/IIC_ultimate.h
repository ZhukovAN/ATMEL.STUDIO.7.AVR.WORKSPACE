#ifndef IICULTIMATE_H
#define IICULTIMATE_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <inttypes.h>
#include <stddef.h>

#define i2c_PORT	PORTC				// Порт где сидит нога TWI
#define i2c_DDR		DDRC
#define i2c_SCL		0					// Биты соответствующих выводов
#define i2c_SDA		1

#define i2c_MasterAddress 	0x32		// Адрес на который будем отзываться
#define i2c_i_am_slave		1			// Если мы еще и слейвом работаем то 1. А то не услышит!

#define i2c_MasterBytesRX	1			// Величина принимающего буфера режима Slave, т.е. сколько байт жрем.
#define i2c_MasterBytesTX	1			// Величина Передающего буфера режима Slave , т.е. сколько байт отдаем за сессию.

#define i2c_MaxBuffer		8			// Величина буфера Master режима RX-TX. Зависит от того какой длины строки мы будем гонять
#define i2c_MaxPageAddrLgth	2			// Максимальная величина адреса страницы. Обычно адрес страницы это один или два байта. 
										// Зависит от типа ЕЕПРОМ или другой микросхемы. 

#define i2c_type_msk	0b00001100		// Маска режима
#define i2c_sarp		0b00000000		// Start-Addr_R-Read-Stop  							Это режим простого чтения. Например из слейва или из епрома с текущего адреса
#define i2c_sawp		0b00000100		// Start-Addr_W-Write-Stop 							Это режим простой записи. В том числе и запись с адресом страницы. 
#define i2c_sawsarp		0b00001000		// Start-Addr_W-WrPageAdr-rStart-Addr_R-Read-Stop 	Это режим с предварительной записью текущего адреса страницы

#define i2c_Err_msk		0b00110011		// Маска кода ошибок
#define i2c_Err_NO		0b00000000		// All Right! 						-- Все окей, передача успешна. 
#define i2c_ERR_NA		0b00010000		// Device No Answer 				-- Слейв не отвечает. Т.к. либо занят, либо его нет на линии.
#define i2c_ERR_LP		0b00100000		// Low Priority 					-- нас перехватили собственным адресом, либо мы проиграли арбитраж
#define i2c_ERR_NK		0b00000010		// Received NACK. End Transmittion. -- Был получен NACK. Бывает и так.
#define i2c_ERR_BF		0b00000001		// BUS FAIL 						-- Автобус сломался. И этим все сказано. Можно попробовать сделать переинициализацию шины

#define i2c_Interrupted		0b10000000	// Transmiting Interrupted		Битмаска установки флага занятости
#define i2c_NoInterrupted 	0b01111111  // Transmiting No Interrupted	Битмаска снятия флага занятости

#define i2c_Busy		0b01000000  	// Trans is Busy				Битмаска флага "Передатчик занят, руками не трогать". 
#define i2c_Free		0b10111111  	// Trans is Free				Битмаска снятия флага занятости.

#ifdef I2C_MASTER
#define MACRO_i2c_WhatDo_MasterOut 	(MasterOutFunc)();		// Макросы для режимо выхода. Пока тут функция, но может быть что угодно
#endif
#ifdef I2C_SLAVE
#define MACRO_i2c_WhatDo_SlaveOut   (SlaveOutFunc)();
#endif
#define MACRO_i2c_WhatDo_ErrorOut   (ErrorOutFunc)();


typedef void (*IIC_F)(void);								// Тип указателя на функцию
// Мои доработки
// Необходимость в них возникла при написании модуля взаимодействия с HD44780 
// посредством I2C-расширителя PCF8574. Максимальная частота работы PCF8574 составляет 100kHz, 
// поэтому на передачу одного байта уходит порядка 100µS. Типовой тайминг HD44780 составляет 37µS,
// при этом на передачу одной порции данных уходит три байта (см. рис. 9 на стр. 22 описания HD44780):
// 1: Rs - low, D4-D7 - data	 
// 2: Rs - high, D4-D7 - data
// 3: Rs - low, D4-D7 - data
// Таким образом, на одну порцию данных затрачивается не менее 300µS, соответственно, вносить 
// дополнительные задержки не требуется. Это позволяет при передаче буфера отправлять данные не 
// байт за байтом (что приводит к дополнительным расходам в виде отправки I2C-адресного байта при
// каждой посылке), а подготавливать буфер и отправлять его целиком.
// Однако, решение этой задачи "в лоб" будет приводить к тому, что подготавливаемый буфер должен 
// будет содержать в шесть раз больше данных, нежели передаваемая строка: три байта на одну 
// четырехбитную посылку умножаются на два для передачи байта. Такой подход слишком расточителен,
// поэтому код DI HALT'а был доработан с тем, чтобы обеспечивать возможность вычисления передаваемых 
// значений в callback-функции, вызываемой непосредственно из кода обработчика прерывания. Указанная 
// функция принимает на вход номер передаваемого байта и возвращает значение, которое должно быть 
// отправлено
#ifdef I2C_MASTER
typedef uint8_t (*IIC_EXT)(uint16_t theIdx);
extern IIC_EXT GetDataFunc;
extern IIC_F MasterOutFunc;
#endif
#ifdef I2C_SLAVE									// Подрбрости в сишнике. 
extern IIC_F SlaveOutFunc;
#endif
extern IIC_F ErrorOutFunc;

extern uint8_t i2c_Do;	

#ifdef I2C_SLAVE
extern uint8_t i2c_InBuff[i2c_MasterBytesRX];
extern uint8_t i2c_OutBuff[i2c_MasterBytesTX];
extern uint8_t i2c_SlaveIndex;
extern void Init_Slave_i2c(IIC_F Addr);
#endif

#ifdef I2C_MASTER
extern uint8_t i2c_Buffer[i2c_MaxBuffer];
extern uint8_t i2c_index;
extern uint8_t i2c_ByteCount;

extern uint8_t i2c_SlaveAddress;
extern uint8_t i2c_PageAddress[i2c_MaxPageAddrLgth];

extern uint8_t i2c_PageAddrIndex;
extern uint8_t i2c_PageAddrCount;
extern void Init_i2c(void);
#endif

#endif
