/*
 * HD44780.h
 *
 * Created: 06.09.2017 18:31:23
 *  Author: Alexey N. Zhukov
 */ 


#ifndef HD44780_V1_H_
#define HD44780_V1_H_

#include <inttypes.h>
#include "IIC_ultimate.h"

class HD44780 {
	public:
		HD44780(uint8_t theAddr, uint8_t theCols, uint8_t theRows);
		~HD44780();
		void begin();
		static HD44780* g_ptrActiveLCD;
		bool command(uint8_t theCmd, bool the4BitOnlyMode = false);
		bool data(uint8_t theData, bool the4BitOnlyMode = false);
		bool display();
		bool clear();
		bool home();
		bool printAt(uint8_t theCol, uint8_t theRow, char* theData, uint8_t theLength);
	protected:
		uint8_t m_intAddr;
		uint8_t m_intCols;
		uint8_t m_intRows;
		// Буфер данных и его максимальный и текущий размеры
		uint8_t* m_ptrBuffer;
		uint8_t m_intMaxBufferSize;
		uint8_t m_intBufferSize;
		// Команда. Если 0, значит передаются данные (бит RS должен быть установлен в 1
		uint8_t m_intCmd;		
		uint8_t m_intDisplayControl;
		uint8_t m_intDisplayMode;
		uint8_t m_intDisplayFunction;	
		uint8_t m_intCharSize;	
	private:
		// Запуск I2C-передачи команды (если не 0) и данных:
		// В синхронном и асинхронном режимах
		bool send();
		bool send(IIC_F onSendComplete);
		bool m_bool4BitOnlyMode;
		// Callback, вызываемый по завершении передачи
		IIC_F m_ptrOnSendComplete;
		volatile bool m_boolSendComplete;
		// Перенос функциональности в отдельный класс реализован для того, 
		// чтобы иметь возможность работы с несколькими LCD-экранами. 
		// Но поскольку все они используют одну I2C-шину, то для решения 
		// проблемы с синхронизацией, когда начинается передача данных на второй
		// экран когда еще не завершилась передача на второй, требуются своеобразные семафоры
		// Второй момент - передача одного байта по I2C на экран, работающий в 
		// четырехбитном режиме, требует последовательной передачи шести байт из-за того, что 
		// используется манипуляция битом En. Решение этой задачи в лоб потребует или увеличивать 
		// в шесть раз буфер I2C или разбивать передачу данных на несколько последовательных
		// Обы этих варианта не являются привлекательными, поэтому реализация I2C от Di HALT
		// была расширена: в нее добавлена возможность вызова callback-функции, возвращающей 
		// значение, которое необходимо передать. Это позволяет, во-первых, вынести буфер данных, 
		// передаваемых по I2C за рамки ее реализации, а, во-вторых, оптимизировать размер этого 
		// буфера за счет поддержки вычисления значения передаваемого байта данных "на лету"
		bool lockI2C();
		bool releaseI2C();
		// Не хочется давать доступ к getData откуда-либо кроме getDataFunc
		friend uint8_t GetData(uint16_t theIdx);
		// Аналогично с OnSendComplete
		friend void OnSendComplete();
		// Метод, вызываемый по завершении I2C-передачи
		virtual void onSendComplete();
		uint8_t getData(uint16_t theIdx);
};

#endif /* HD44780_V1_H_ */