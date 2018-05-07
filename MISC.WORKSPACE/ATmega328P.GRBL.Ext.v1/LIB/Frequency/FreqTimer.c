/*
 * FreqTimer.c
 *
 * Created: 04.09.2017 8:44:13
 *  Author: Alexey N. Zhukov
 */ 

#include <avr/interrupt.h>
#include <stdbool.h>

#include "FreqTimer.h"
#include "EERTOS.h"
#include "DebugTools.h"

void TimeOutWatchDog();
void OnCaptureComplete(void);
float Frequency();

static float g_fltFrequency = 0;

// ������� ������� �������. ��������� ��� ������ ������� ������� "��������������" ������ ������, ��
// � ���� �������� �� ������������ ����� OnCaptureComplete
static volatile bool g_boolFirstCapture;

// ��������� ����:
// ���������� ������ �������� ������������� �� ����� 
// while (BitIsClear(TIFR, ICF1));
// ��������, ��� ��� ����� ������� MCU ������ ����� �� ��������, ��� �������� ������� ��� 
// ��������� ������� ����������� �������. ����� ��� ��������� � ���, ����� ����������� 
// ������ ����������� ������� ����������� �� ���������� TIMER1_CAPT_vect

void FreqTimerInit(void) {
    g_boolFirstCapture = true;
	// ������������� ������� �0
	// - ���������� ���������� �� ������������ (page 109)
	TIMSK0 |= (1 << TOIE0);
	// - �������� ������ - ������� � ������ �0 (page 107)
	TCCR0B = (1 << CS02) | (1 << CS01) | (1 << CS00);
	TCNT0 = 0;

    // ������������� ������� �1
    // - ���������� ���������� �� ������������ (TOIE1) � �� ������� (ICIE1)
    // - ����� normal (COM1A1:0 = 00, WGM13:0 = 0000)
    // - ������ �� ��������� ������ (ICES1), �������������� "��������" (ICNC1) ���������
    // - ������������ 1 (CS12:0 = 001)
    TIMSK1 |= (1 << TOIE1) | (1 << ICIE1);
    TCCR1A = (0 << COM1A1) | (0 << COM1A0) | (0 << WGM11) | (0 << WGM10);
    TCCR1B = (0 << ICNC1) | (1 << ICES1) | (0 << WGM13) | (0<<WGM12) | (0 << CS12) | (0 << CS11) | (1 << CS10);
    TCNT1 = 0;

	// ��������� Watchdog ��� ��������� ���������� �������� ������� (���� � ������� ���������
	// �������� �� ���������� ����� �������� �������, �������� ������� g_fltFrequency)
	SetTask(TimeOutWatchDog);
	SetTask(d5on);
}

// �������� ������������ ��������
volatile unsigned int g_intTimerOvf0 = 0;
volatile unsigned int g_intTimerOvf1 = 0;

ISR(TIMER0_OVF_vect) {
	g_intTimerOvf0++;
}

ISR(TIMER1_OVF_vect) {
	g_intTimerOvf1++;
}

// �������� � �� ����� ��������� ����������� �������
#define FREQ_RES 1000
// ��� ���������� �������� ������ ���������: ������� �� ������ ������� ��������� 
// ����������� �������, �� � ��������� ��� �������. ����������, �������� �������� 
// ����������� �� �� ����� ������ Capture, � � ������ ������� ������ �������. 
// ��� ����������� ��������, �������� ��� ����� ������, �� �������� � ������������� 
// �������������� ��������� �������� � ��������� ����� �� ���� �� ��������� ������

// �������� �������� �� ���������� �������� � ������� Capture �� �������, ��� ������� 
// ������ �� ��������
#define FREQ_TIMEOUT    5 * FREQ_RES
// Watchdog ��������� ��������. �������� �����: � ���������� WD_FREQ � ����� ���������� timeOutWatchDog
// ������ ������� ���������� ���������� �������� g_intWatchDogCounter. �� ���������� ��������� �������� FREQ_TIMEOUT/WD_FREQ
// �������, ��� ������� ������ �� �������� 
#define WD_FREQ         100
#define WD_MAX          FREQ_TIMEOUT / WD_FREQ
volatile uint8_t g_intWatchDogCounter = 0;

void TimeOutWatchDog() {
	g_intWatchDogCounter++;
	if (WD_MAX == g_intWatchDogCounter)
		g_fltFrequency = 0;
    SetTimerTask(TimeOutWatchDog, 100);
}

inline void Capture() {
	// �������� ���������� TIMER1_CAPT_vect
	TIMSK1 |= (1<<ICIE1);
    SetTimerTask(Capture, FREQ_RES);
}

// ������ ��� �������� �������� � ����������� �������� ��������� ����� � ������������
// ���������� ����� � ������������ ������� 1 �� ������ ������������ ���������� � �������������� ��������
volatile uint32_t g_intPrevTimerOvf1 = 0, g_intCurrentTimerOvf1 = 0;
volatile uint16_t g_intPrevTimerTick1 = 0, g_intCurrentTimerTick1 = 0;
// ���������� ����� � ������������ ������� 0 �� ������ ������������ ���������� � �������������� ��������
volatile uint32_t g_intPrevTimerOvf0 = 0, g_intCurrentTimerOvf0 = 0;
volatile uint8_t g_intPrevTimerTick0 = 0, g_intCurrentTimerTick0 = 0;

// ��������� ���������� ����� � ������������ �� ������ �������
ISR(TIMER1_CAPT_vect) {
	g_intPrevTimerTick0 = g_intCurrentTimerTick0;
	g_intCurrentTimerTick0 = TCNT0;
	g_intPrevTimerOvf0 = g_intCurrentTimerOvf0;
	g_intCurrentTimerOvf0 = g_intTimerOvf0;
	
	g_intPrevTimerOvf1 = g_intCurrentTimerOvf1;
	g_intCurrentTimerOvf1 = g_intTimerOvf1;
	g_intPrevTimerTick1 = g_intCurrentTimerTick1;
	g_intCurrentTimerTick1 = TCNT1;
	// ������ ����� ���������� - ��������� ������ ����������� �������, ������� 
	// �� ����� ��� ������������ �� ��� ���������: ��� ������� ��������� � ���������� ����������
	// � �� ����� ������������ ��� ������� �������
	TIMSK1 &= ~(1<<ICIE1);
    SetTask(d2on);
	if (g_boolFirstCapture) 
		g_boolFirstCapture = false;
	else
		SetTask(OnCaptureComplete);
	g_intWatchDogCounter = 0;
}

#ifdef DEBUG
uint32_t g_intTemp0;
uint32_t g_intTemp1;
uint32_t g_intTemp2;
uint32_t g_intTemp3;
uint32_t g_intTemp4;
uint32_t g_intTemp5;
uint32_t g_intTemp6;
uint32_t g_intTemp7;
#endif

float Frequency() {
	cli();
	// ��������� ���������� ��������� ��������� �������, ��������� ����� ����� ���������� ���������
	float l_fltBaseImp = (g_intCurrentTimerTick1 + (uint32_t)(g_intCurrentTimerOvf1 - g_intPrevTimerOvf1) * 65536) - g_intPrevTimerTick1;
	// ��������� ���������� ��������� ����������� �������
	float l_fltMeasuredImp = (g_intCurrentTimerTick0 + (uint32_t)(g_intCurrentTimerOvf0 - g_intPrevTimerOvf0) * 256) - g_intPrevTimerTick0;

    #ifdef DEBUG
    g_intTemp0 = g_intCurrentTimerTick1;
    g_intTemp1 = g_intCurrentTimerOvf1;
    g_intTemp2 = g_intPrevTimerTick1;
    g_intTemp3 = g_intPrevTimerOvf1;
    g_intTemp4 = g_intCurrentTimerTick0;
    g_intTemp5 = g_intCurrentTimerOvf0;
    g_intTemp6 = g_intPrevTimerTick0;
    g_intTemp7 = g_intPrevTimerOvf0;
    #endif
	sei();
    SetTask(d3on);
		
	// ��������� �������� �������
	return (float)(F_CPU) * l_fltMeasuredImp / l_fltBaseImp;
}

inline void OnCaptureComplete(void) {
    g_fltFrequency = Frequency();
}

inline float GetFrequency() {
    return g_fltFrequency;
}