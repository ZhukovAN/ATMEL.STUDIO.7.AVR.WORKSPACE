#include <EERTOS.h>

// Очереди задач, таймеров.
// Тип данных - указатель на функцию
static TPTR	TaskQueue[TaskQueueSize+1];			// очередь указателей
static struct {
	TPTR GoToTask; 						// Указатель перехода
	uint16_t Time;							// Выдержка в мс
}
MainTimer[MainTimerQueueSize + 1];	// Очередь таймеров


// RTOS Подготовка. Очистка очередей
inline void InitRTOS(void) {
	uint8_t	i;

	for (i = 0; i != TaskQueueSize + 1; i++)	// Во все позиции записываем Idle
		TaskQueue[i] = Idle;

	for (i = 0; i != MainTimerQueueSize + 1; i++) { 
		// Обнуляем все таймеры.
		MainTimer[i].GoToTask = Idle;
		MainTimer[i].Time = 0;
	}
}

// Пустая процедура - простой ядра.
inline void Idle(void) {}

// Функция установки задачи в очередь. Передаваемый параметр - указатель на функцию
// Отдаваемое значение - код ошибки.
void SetTask(const TPTR TS) {

	uint8_t		i = 0;
	uint8_t		nointerrupted = 0;

	if (STATUS_REG & (1 << Interrupt_Flag)) {
		 // Если прерывания разрешены, то запрещаем их.
		Disable_Interrupt
		// И ставим флаг, что мы не в прерывании.
		nointerrupted = 1;	
	}

	while (Idle != TaskQueue[i]) {
		// Прочесываем очередь задач на предмет свободной ячейки
		// с значением Idle - конец очереди.
		i++;
		if (TaskQueueSize + 1 == i) {
			// Если очередь переполнена то выходим не солоно хлебавши
			if (nointerrupted)	Enable_Interrupt 	// Если мы не в прерывании, то разрешаем прерывания
			return;									// Раньше функция возвращала код ошибки - очередь переполнена. Пока убрал.
		}
	}
	// Если нашли свободное место, то записываем в очередь задачу
	TaskQueue[i] = TS;							
	// И включаем прерывания если не в обработчике прерывания.
	if (nointerrupted) Enable_Interrupt				
}


// Функция установки задачи по таймеру. Передаваемые параметры - указатель на функцию,
// Время выдержки в тиках системного таймера. Возвращает код ошибки.
void SetTimerTask(const TPTR TS, const uint16_t NewTime) {
	uint8_t		i = 0;
	uint8_t		nointerrupted = 0;
	uint8_t		Idle_i = 0;

	if (STATUS_REG & (1<<Interrupt_Flag)) {
		// Проверка запрета прерывания, аналогично функции выше
		Disable_Interrupt
		nointerrupted = 1;
	}


	for (i = 0; i != MainTimerQueueSize + 1; ++i) {
		//Прочесываем очередь таймеров
		if (TS == MainTimer[i].GoToTask) {
			// Если уже есть запись с таким адресом
			MainTimer[i].Time = NewTime;			// Перезаписываем ей выдержку
			if (nointerrupted) 	Enable_Interrupt		// Разрешаем прерывания если не были запрещены.
			return;										// Выходим. Раньше был код успешной операции. Пока убрал
		} else {
			if ((MainTimer[i].GoToTask == Idle) && (Idle_i == 0))
				Idle_i = i;
		}
	}
	

	for (i = 0; i != MainTimerQueueSize + 1; ++i) {
		// Если не находим похожий таймер, то ищем любой пустой
		if (MainTimer[i].GoToTask == Idle) {
			MainTimer[i].GoToTask = TS;			// Заполняем поле перехода задачи
			MainTimer[i].Time = NewTime;		// И поле выдержки времени
			if (nointerrupted) Enable_Interrupt		// Разрешаем прерывания
			return;									// Выход.
		}
	}												// тут можно сделать return c кодом ошибки - нет свободных таймеров
}

/*=================================================================================
Диспетчер задач ОС. Выбирает из очереди задачи и отправляет на выполнение.
*/
inline void TaskManager(void) {
	TPTR	GoToTask = Idle;		// Инициализируем переменные

	Disable_Interrupt				// Запрещаем прерывания!!!
	GoToTask = TaskQueue[0];		// Хватаем первое значение из очереди

	if (GoToTask == Idle) {
		// Если там пусто
		Enable_Interrupt			// Разрешаем прерывания
		(Idle)(); 					// Переходим на обработку пустого цикла
	} else {
		for (uint8_t i = 0; i != TaskQueueSize; i++)	// В противном случае сдвигаем всю очередь
			TaskQueue[i]=TaskQueue[i + 1];

		TaskQueue[TaskQueueSize] = Idle;				// В последнюю запись пихаем затычку

		Enable_Interrupt							// Разрешаем прерывания
		(GoToTask)();								// Переходим к задаче
	}
}

/*
Служба таймеров ядра. Должна вызываться из прерывания раз в 1мс. Хотя время можно варьировать в зависимости от задачи

To DO: Привести к возможности загружать произвольную очередь таймеров. Тогда можно будет создавать их целую прорву.
А также использовать эту функцию произвольным образом.
В этом случае не забыть добавить проверку прерывания.
*/
inline void TimerService(void) {
	for (uint8_t i = 0; i != MainTimerQueueSize + 1; i++) {
		// Прочесываем очередь таймеров
		if (MainTimer[i].GoToTask == Idle) continue;	// Если нашли пустышку - щелкаем следующую итерацию

		// Если таймер не выщелкал, то щелкаем еще раз.
		// To Do: Вычислить по тактам, что лучше !=1 или !=0.
		// Уменьшаем число в ячейке если не конец
		if (MainTimer[i].Time != 1)						
			MainTimer[i].Time--;						
		else {
			SetTask(MainTimer[i].GoToTask);				// Дощелкали до нуля? Пихаем в очередь задачу
			MainTimer[i].GoToTask = Idle;				// А в ячейку пишем затычку
		}
	}
}

ISR(RTOS_ISR) {
	TimerService();
}