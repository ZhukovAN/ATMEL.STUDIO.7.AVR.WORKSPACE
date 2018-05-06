#include "IIC_ultimate.h"

void DoNothing(void);

uint8_t i2c_Do;		
										// Переменная состояния передатчика IIC
#ifdef I2C_SLAVE
uint8_t i2c_InBuff[i2c_MasterBytesRX];		// Буфер прием при работе как Slave
uint8_t i2c_OutBuff[i2c_MasterBytesTX];		// Буфер передачи при работе как Slave
uint8_t i2c_SlaveIndex;						// Индекс буфера Slave
#endif

#ifdef I2C_MASTER
uint8_t i2c_Buffer[i2c_MaxBuffer];			// Буфер для данных работы в режиме Master
uint8_t i2c_index;							// Индекс этого буфера
uint8_t i2c_ByteCount;						// Число байт передаваемых

uint8_t i2c_SlaveAddress;					// Адрес подчиненного

uint8_t i2c_PageAddress[i2c_MaxPageAddrLgth];	// Буфер адреса страниц (для режима с sawsarp)
uint8_t i2c_PageAddrIndex;						// Индекс буфера адреса страниц
uint8_t i2c_PageAddrCount;						// Число байт в адресе страницы для текущего Slave

IIC_EXT GetDataFunc = NULL;
#endif

// Указатели выхода из автомата:
#ifdef I2C_MASTER
IIC_F MasterOutFunc = &DoNothing;			//  в Master режиме
#endif

#ifdef I2C_SLAVE
IIC_F SlaveOutFunc 	= &DoNothing;			//  в режиме Slave
#endif
IIC_F ErrorOutFunc 	= &DoNothing;			//  в результате ошибки в режиме Master

ISR(TWI_vect) {								
	// Прерывание TWI Тут наше все.
	switch (TWSR & 0xF8) {						// Отсекаем биты прескалера
		case 0x00: {
			// Bus Fail (автобус сломался)
			i2c_Do |= i2c_ERR_BF;
			TWCR = 0<<TWSTA|1<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;  	// Go!
			MACRO_i2c_WhatDo_ErrorOut
			break;
		}

#ifdef I2C_MASTER
		case 0x08: {
			// Старт был, а затем мы:
			uint8_t l_intTemp = i2c_SlaveAddress << 1;
			if (i2c_sarp == (i2c_Do & i2c_type_msk))						// В зависимости от режима
				l_intTemp |= 0x01;											// Шлем Addr+R
			else															// Или
				l_intTemp &= 0xFE;											// Шлем Addr+W
			
			TWDR = l_intTemp;												// Адрес слейва
			TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;  	// Go!
			break;
		}
		
		case 0x10: {
			// Повторный старт был, а затем мы
			uint8_t l_intTemp = i2c_SlaveAddress << 1;
			if (i2c_sawsarp == (i2c_Do & i2c_type_msk))						// В зависимости от режима
				l_intTemp |= 0x01;									// Шлем Addr+R
			else
				l_intTemp &= 0xFE;									// Шлем Addr+W

			// To Do: Добавить сюда обработку ошибок

			TWDR = l_intTemp;													// Адрес слейва
			TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;  	// Go!
			break;
		}

		case 0x18: {
			// Был послан SLA+W получили ACK, а затем:
			if (i2c_sawp == (i2c_Do & i2c_type_msk)) {						// В зависимости от режима
				// Шлем байт данных
				if (NULL == GetDataFunc)
					// Берем из буфера
					TWDR = i2c_Buffer[i2c_index];							
				else
					// Или обращаемся к callback-функции
					TWDR = (GetDataFunc)(i2c_index);
				i2c_index++;												// Увеличиваем указатель буфера
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;  // Go!
			}
			if (i2c_sawsarp == (i2c_Do & i2c_type_msk))	{
				TWDR = i2c_PageAddress[i2c_PageAddrIndex];					// Или шлем адрес странцы (по сути тоже байт данных)
				i2c_PageAddrIndex++;										// Увеличиваем указатель буфера страницы
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;	// Go!
			}
			break;
		}

		case 0x20: {
			// Был послан SLA+W получили NACK - слейв либо занят, либо его нет дома.
			i2c_Do |= i2c_ERR_NA;															// Код ошибки
			TWCR = 0<<TWSTA|1<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;			// Шлем шине Stop

			MACRO_i2c_WhatDo_ErrorOut 														// Обрабатываем событие ошибки;
			break;
		}

		case 0x28: {
			// Байт данных послали, получили ACK!  (если sawp - это был байт данных. если sawsarp - байт адреса страницы)
			// А дальше:
			if (i2c_sawp == (i2c_Do & i2c_type_msk)) {
				// В зависимости от режима
				if (i2c_index == i2c_ByteCount) {
					// Если был байт данных последний
					TWCR = 0<<TWSTA|1<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;	// Шлем Stop
					MACRO_i2c_WhatDo_MasterOut												// И выходим в обработку стопа
				} else {
					if (NULL == GetDataFunc)
						// Берем из буфера
						TWDR = i2c_Buffer[i2c_index];
					else
						// Или обращаемся к callback-функции
						TWDR = (GetDataFunc)(i2c_index);
					i2c_index++;
					TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;  	// Go!
				}
			}

			if (i2c_sawsarp == (i2c_Do & i2c_type_msk)) {
				// В другом режиме мы
				if(i2c_PageAddrIndex == i2c_PageAddrCount)
					// Если последний байт адреса страницы запускаем Повторный старт!
					TWCR = 1<<TWSTA|0<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;		
				else {														
					// Иначе шлем еще один адрес страницы
					TWDR = i2c_PageAddress[i2c_PageAddrIndex];				
					// Увеличиваем индекс счетчика адреса страниц
					i2c_PageAddrIndex++;									
					TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;		// Go!
				}
			}
			break;
		}

		case 0x30: {	
			//Байт ушел, но получили NACK причин две. 1я передача оборвана слейвом и так надо. 2я слейв сглючил.
			i2c_Do |= i2c_ERR_NK;				// Запишем статус ошибки. Хотя это не факт, что ошибка.
			TWCR = 0<<TWSTA|1<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;		// Шлем Stop
			MACRO_i2c_WhatDo_MasterOut													// Отрабатываем событие выхода
			break;
		}

		case 0x38: {	
			// Коллизия на шине. Нашелся кто то поглавней
			// Ставим ошибку потери приоритета
			i2c_Do |= i2c_ERR_LP;			

			// Настраиваем индексы заново.
			i2c_index = 0;
			i2c_PageAddrIndex = 0;

			TWCR = 1<<TWSTA|0<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;		// Как только шина будет свободна
			break;																		// попробуем передать снова.
		}

		case 0x40: {
			// Послали SLA+R получили АСК. А теперь будем получать байты
			if (i2c_index + 1 == i2c_ByteCount) 
				// Если буфер кончится на этом байте, то
				// Требуем байт, а в ответ потом пошлем NACK(Disconnect)
				// Что даст понять слейву, что мол хватит гнать. И он отпустит шину
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|0<<TWEA|1<<TWEN|1<<TWIE;	
			else
				// Или просто примем байт и скажем потом ACK
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;	
			break;
		}

		case 0x48: {
			// Послали SLA+R, но получили NACK. Видать slave занят или его нет дома.
			i2c_Do |= i2c_ERR_NA;															// Код ошибки No Answer
			TWCR = 0<<TWSTA|1<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;			// Шлем Stop

			MACRO_i2c_WhatDo_ErrorOut														// Отрабатываем выходную ситуацию ошибки
			break;
		}

		case 0x50: {
			// Приняли байт.
			i2c_Buffer[i2c_index] = TWDR;			// Забрали его из буфера
			i2c_index++;

			// To Do: Добавить проверку переполнения буфера. А то мало ли что юзер затребует

			if (i2c_index + 1 == i2c_ByteCount)		
				// Если остался еще один байт из тех, что мы хотели считать
				// Затребываем его и потом пошлем NACK (Disconnect)
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|0<<TWEA|1<<TWEN|1<<TWIE;		
			else
				// Если нет, то затребываем следующий байт, а в ответ скажем АСК
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;		
			break;
		}

		case 0x58: {
			// Вот мы взяли последний байт, сказали NACK слейв обиделся и отпал.
			i2c_Buffer[i2c_index] = TWDR;													// Взяли байт в буфер
			TWCR = 0<<TWSTA|1<<TWSTO|1<<TWINT|i2c_i_am_slave<<TWEA|1<<TWEN|1<<TWIE;			// Передали Stop
			MACRO_i2c_WhatDo_MasterOut														// Отработали точку выхода
			break;
		}
#endif

#ifdef I2C_SLAVE
		// IIC  Slave ============================================================================
		case 0x68:	// RCV SLA+W Low Priority						// Словили свой адрес во время передачи мастером
		case 0x78: {
			// RCV SLA+W Low Priority (Broadcast)					// Или это был широковещательный пакет. Не важно
			i2c_Do |= i2c_ERR_LP | i2c_Interrupted;					// Ставим флаг ошибки Low Priority, а также флаг того, что мастера прервали
			
			// Restore Trans after.
			i2c_index = 0;											// Подготовили прерваную передачу заново
			i2c_PageAddrIndex = 0;
		}															// И пошли дальше. Внимание!!! break тут нет, а значит идем в "case 60"

		case 0x60: // RCV SLA+W  Incoming?							// Или просто получили свой адрес
		case 0x70: {
			// RCV SLA+W  Incoming? (Broascast)						// Или широковещательный пакет
			i2c_Do |= i2c_Busy;										// Занимаем шину. Чтобы другие не совались
			i2c_SlaveIndex = 0;										// Указатель на начало буфера слейва, Неважно какой буфер. Не ошибемся

			if (1 == i2c_MasterBytesRX)								
				// Если нам суждено принять всего один байт, то готовимся принять  его
				// Принять и сказать пошли все н... NACK!
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|0<<TWEA|1<<TWEN|1<<TWIE;			
			else
				// А если душа шире чем один байт, то сожрем и потребуем еще ACK!
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;			
			break;
		}

		case 0x80:	// RCV Data Byte									// И вот мы приняли этот байт. Наш или широковещательный. Не важно
		case 0x90: {
			// RCV Data Byte (Broadcast)
			i2c_InBuff[i2c_SlaveIndex] = TWDR;						// Сжираем его в буфер.
			i2c_SlaveIndex++;										// Сдвигаем указатель
			if (i2c_SlaveIndex == i2c_MasterBytesRX-1) 				
				// Свободно место всего под один байт?
				// Принять его и сказать NACK!
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|0<<TWEA|1<<TWEN|1<<TWIE;			
			else
				// Места еще дофига? Принять и ACK!
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;			
			break;
		}

		case 0x88: // RCV Last Byte										// Приняли последний байт
		case 0x98: {
			// RCV Last Byte (Broadcast)
			i2c_InBuff[i2c_SlaveIndex] = TWDR;						// Сожрали его в буфер
			if (i2c_Do & i2c_Interrupted)							
				// Если у нас был прерываный сеанс от имени мастера
				// Влепим в шину свой Start поскорей и сделаем еще одну попытку
				TWCR = 1<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;			
			else
				// Если не было такого факта, то просто отвалимся и будем ждать
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;			
			// И лениво отработаем наш выходной экшн для слейва
			MACRO_i2c_WhatDo_SlaveOut												
			break;
		}
		
		case 0xA0: {
			// Ой, мы получили Повторный старт. Но чо нам с ним делать?
			// Можно, конечно, сделать вспомогательный автомат, чтобы обрабатывать еще и адреса внутренних страниц, подобно еепромке.
			// Но я не стал заморачиваться. В этом случае делается это тут.
			TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;			// просто разадресуемся, проигнорировав этот посыл
			break;
		}

		case 0xB0: {
			// Поймали свой адрес на чтение во время передачи Мастером
			// Ну чо, коды ошибки и флаг прерваной передачи.
			i2c_Do |= i2c_ERR_LP | i2c_Interrupted;			
			// Восстанавливаем индексы
			i2c_index = 0;
			i2c_PageAddrIndex = 0;
		}												// Break нет! Идем дальше

		case 0xA8: {
			// Либо просто словили свой адрес на чтение
			// Индексы слейвовых массивов на 0
			i2c_SlaveIndex = 0;								
			// Чтож, отдадим байт из тех что есть.
			TWDR = i2c_OutBuff[i2c_SlaveIndex];				
			
			if(i2c_MasterBytesTX == 1)
				// Если он последний, мы еще на NACK в ответ надеемся
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|0<<TWEA|1<<TWEN|1<<TWIE;	
			else
				// А если нет, то  ACK ждем
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;

			break;
		}
		
		case 0xB8: {
			// Послали байт, получили ACK
			// Значит продолжаем дискотеку. Берем следующий байт
			i2c_SlaveIndex++;								
			// Даем его мастеру
			TWDR = i2c_OutBuff[i2c_SlaveIndex];				

			if (i2c_SlaveIndex == i2c_MasterBytesTX-1)		
				// Если он последний был, то
				// Шлем его и ждем NACK
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|0<<TWEA|1<<TWEN|1<<TWIE;
			else
				// Если нет, то шлем и ждем ACK
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;

			break;
		}

		case 0xC0: // Мы выслали последний байт, больше у нас нет, получили NACK
		case 0xC8: {
			// или ACK. В данном случае нам пох. Т.к. больше байтов у нас нет.
			if (i2c_Do & i2c_Interrupted) {
				// Если там была прерваная передача мастера
				// То мы ему ее вернем
				i2c_Do &= i2c_NoInterrupted;										// Снимем флаг прерваности
				TWCR = 1<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;			// Сгенерим старт сразу же как получим шину.
			} else
				// Если мы там одни, то просто отдадим шину
				TWCR = 0<<TWSTA|0<<TWSTO|1<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;

			// И отработаем выход слейва. Впрочем, он тут
			// Не особо то нужен. Разве что как сигнал, что мастер
			// Нас почтил своим визитом.
			MACRO_i2c_WhatDo_SlaveOut												
			break;																	
		}
#endif
		default: break;
	}
}

// Функция пустышка, затыкать несуществующие ссылки
void DoNothing(void) {}

#ifdef I2C_MASTER
// Настройка режима мастера
void Init_i2c(void)	{
	// Включим подтяжку на ноги, вдруг юзер на резисторы пожмотился
	i2c_PORT |= 1<<i2c_SCL|1<<i2c_SDA;			
	i2c_DDR &= ~(1<<i2c_SCL|1<<i2c_SDA);

	// Prescaler is 64 (= 4^3 = 4^0b00000011)
	// TWSR = 1<<TWPS1|1<<TWPS0;				
	// Prescaler is 1  (= 4^0 = 4^0b0000000)	
	TWSR = 0;									
	// SCL freq = F_CPU / (16+2×TWBR×Prescaler)
	// For 400 kHz SCL frequency
	// 2×TWBR×Prescaler = F_CPU / SCL - 16
	// Настроим битрейт 100kHz (столько по datasheet поддерживает I2C expander PCF8574)
	TWBR = 72;         							
}
#endif

#ifdef I2C_SLAVE
// Настройка режима слейва (если нужно)
void Init_Slave_i2c(IIC_F Addr) {
	// Внесем в регистр свой адрес, на который будем отзываться.
	// 1 в нулевом бите означает, что мы отзываемся на широковещательные пакеты
	TWAR = i2c_MasterAddress;					
	// Присвоим указателю выхода по слейву функцию выхода
	SlaveOutFunc = Addr;						
	
	// Включаем агрегат и начинаем слушать шину.
	TWCR = 0<<TWSTA|0<<TWSTO|0<<TWINT|1<<TWEA|1<<TWEN|1<<TWIE;		
}
#endif
