#include <EERTOS.h>

// ������� �����, ��������.
// ��� ������ - ��������� �� �������
static TPTR	TaskQueue[TaskQueueSize+1];			// ������� ����������
static struct {
	TPTR GoToTask; 						// ��������� ��������
	uint16_t Time;							// �������� � ��
}
MainTimer[MainTimerQueueSize + 1];	// ������� ��������


// RTOS ����������. ������� ��������
inline void InitRTOS(void) {
	uint8_t	i;

	for (i = 0; i != TaskQueueSize + 1; i++)	// �� ��� ������� ���������� Idle
		TaskQueue[i] = Idle;

	for (i = 0; i != MainTimerQueueSize + 1; i++) { 
		// �������� ��� �������.
		MainTimer[i].GoToTask = Idle;
		MainTimer[i].Time = 0;
	}
}

// ������ ��������� - ������� ����.
inline void Idle(void) {}

// ������� ��������� ������ � �������. ������������ �������� - ��������� �� �������
// ���������� �������� - ��� ������.
void SetTask(const TPTR TS) {

	uint8_t		i = 0;
	uint8_t		nointerrupted = 0;

	if (STATUS_REG & (1 << Interrupt_Flag)) {
		 // ���� ���������� ���������, �� ��������� ��.
		Disable_Interrupt
		// � ������ ����, ��� �� �� � ����������.
		nointerrupted = 1;	
	}

	while (Idle != TaskQueue[i]) {
		// ����������� ������� ����� �� ������� ��������� ������
		// � ��������� Idle - ����� �������.
		i++;
		if (TaskQueueSize + 1 == i) {
			// ���� ������� ����������� �� ������� �� ������ ��������
			if (nointerrupted)	Enable_Interrupt 	// ���� �� �� � ����������, �� ��������� ����������
			return;									// ������ ������� ���������� ��� ������ - ������� �����������. ���� �����.
		}
	}
	// ���� ����� ��������� �����, �� ���������� � ������� ������
	TaskQueue[i] = TS;							
	// � �������� ���������� ���� �� � ����������� ����������.
	if (nointerrupted) Enable_Interrupt				
}


// ������� ��������� ������ �� �������. ������������ ��������� - ��������� �� �������,
// ����� �������� � ����� ���������� �������. ���������� ��� ������.
void SetTimerTask(const TPTR TS, const uint16_t NewTime) {
	uint8_t		i = 0;
	uint8_t		nointerrupted = 0;
	uint8_t		Idle_i = 0;

	if (STATUS_REG & (1<<Interrupt_Flag)) {
		// �������� ������� ����������, ���������� ������� ����
		Disable_Interrupt
		nointerrupted = 1;
	}


	for (i = 0; i != MainTimerQueueSize + 1; ++i) {
		//����������� ������� ��������
		if (TS == MainTimer[i].GoToTask) {
			// ���� ��� ���� ������ � ����� �������
			MainTimer[i].Time = NewTime;			// �������������� �� ��������
			if (nointerrupted) 	Enable_Interrupt		// ��������� ���������� ���� �� ���� ���������.
			return;										// �������. ������ ��� ��� �������� ��������. ���� �����
		} else {
			if ((MainTimer[i].GoToTask == Idle) && (Idle_i == 0))
				Idle_i = i;
		}
	}
	

	for (i = 0; i != MainTimerQueueSize + 1; ++i) {
		// ���� �� ������� ������� ������, �� ���� ����� ������
		if (MainTimer[i].GoToTask == Idle) {
			MainTimer[i].GoToTask = TS;			// ��������� ���� �������� ������
			MainTimer[i].Time = NewTime;		// � ���� �������� �������
			if (nointerrupted) Enable_Interrupt		// ��������� ����������
			return;									// �����.
		}
	}												// ��� ����� ������� return c ����� ������ - ��� ��������� ��������
}

/*=================================================================================
��������� ����� ��. �������� �� ������� ������ � ���������� �� ����������.
*/
inline void TaskManager(void) {
	TPTR	GoToTask = Idle;		// �������������� ����������

	Disable_Interrupt				// ��������� ����������!!!
	GoToTask = TaskQueue[0];		// ������� ������ �������� �� �������

	if (GoToTask == Idle) {
		// ���� ��� �����
		Enable_Interrupt			// ��������� ����������
		(Idle)(); 					// ��������� �� ��������� ������� �����
	} else {
		for (uint8_t i = 0; i != TaskQueueSize; i++)	// � ��������� ������ �������� ��� �������
			TaskQueue[i]=TaskQueue[i + 1];

		TaskQueue[TaskQueueSize] = Idle;				// � ��������� ������ ������ �������

		Enable_Interrupt							// ��������� ����������
		(GoToTask)();								// ��������� � ������
	}
}

/*
������ �������� ����. ������ ���������� �� ���������� ��� � 1��. ���� ����� ����� ����������� � ����������� �� ������

To DO: �������� � ����������� ��������� ������������ ������� ��������. ����� ����� ����� ��������� �� ����� ������.
� ����� ������������ ��� ������� ������������ �������.
� ���� ������ �� ������ �������� �������� ����������.
*/
inline void TimerService(void) {
	for (uint8_t i = 0; i != MainTimerQueueSize + 1; i++) {
		// ����������� ������� ��������
		if (MainTimer[i].GoToTask == Idle) continue;	// ���� ����� �������� - ������� ��������� ��������

		// ���� ������ �� ��������, �� ������� ��� ���.
		// To Do: ��������� �� ������, ��� ����� !=1 ��� !=0.
		// ��������� ����� � ������ ���� �� �����
		if (MainTimer[i].Time != 1)						
			MainTimer[i].Time--;						
		else {
			SetTask(MainTimer[i].GoToTask);				// ��������� �� ����? ������ � ������� ������
			MainTimer[i].GoToTask = Idle;				// � � ������ ����� �������
		}
	}
}

ISR(RTOS_ISR) {
	TimerService();
}