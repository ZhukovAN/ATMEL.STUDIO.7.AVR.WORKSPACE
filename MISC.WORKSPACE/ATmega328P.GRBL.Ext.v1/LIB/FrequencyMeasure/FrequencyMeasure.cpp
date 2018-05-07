/* FreqMeasure Library, for measuring relatively low frequencies
* http://www.pjrc.com/teensy/td_libs_FreqMeasure.html
* Copyright (c) 2011 PJRC.COM, LLC - Paul Stoffregen <paul@pjrc.com>
*
* Version 1.1
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "FrequencyMeasure.h"
#include "FrequencyMeasureCapture.h"
#include "EERTOS.h"

#define FREQMEASURE_BUFFER_LEN 12

// Кольцевой буфер, в котором хранятся длительности импульсов
static volatile uint32_t buffer_value[FREQMEASURE_BUFFER_LEN];
static volatile uint8_t buffer_head;
static volatile uint8_t buffer_tail;
static uint16_t capture_msw;
static uint32_t capture_previous;

void CFrequencyMeasure::begin(void) {
    capture_init();
    capture_msw = 0;
    capture_previous = 0;
    buffer_head = 0;
    buffer_tail = 0;
    capture_start();
}

uint8_t CFrequencyMeasure::available(void) {
    uint8_t l_intHead, l_intTail;

    l_intHead = buffer_head;
    l_intTail = buffer_tail;
    if (l_intHead >= l_intTail) return l_intHead - l_intTail;

    return FREQMEASURE_BUFFER_LEN + l_intHead - l_intTail;
}

uint32_t CFrequencyMeasure::read(void) {
    uint8_t l_intHead, l_intTail;
    uint32_t l_intValue;

    l_intHead = buffer_head;
    l_intTail = buffer_tail;
    if (l_intHead == l_intTail) return 0xFFFFFFFF;
    l_intTail++;
    if (l_intTail >= FREQMEASURE_BUFFER_LEN) l_intTail = 0;
    l_intValue = buffer_value[l_intTail];
    buffer_tail = l_intTail;
    return l_intValue;
}

float CFrequencyMeasure::countToFrequency(uint32_t theCount) {
    return (float)F_CPU / (float)theCount;
}

float CFrequencyMeasure::countToNanoseconds(uint32_t theCount) {
    return (float)theCount * (1000000000.0f / (float)F_CPU);
}

void CFrequencyMeasure::end(void) {
    capture_shutdown();
}

ISR(TIMER_OVERFLOW_VECTOR) {
    capture_msw++;
}

#include <string.h>
#include <stdio.h>
#include "convert.h"
#include "uartutils.h"
uint32_t g_intLastPeriod = 0;
#define DEBUG_FREQ_BUF_SIZE 64
char g_strFreqOutBuffer[DEBUG_FREQ_BUF_SIZE];
#define DEBUG_FREQ_TEMP_BUF_SIZE 16
char g_strFreqOutTempBuffer[DEBUG_FREQ_TEMP_BUF_SIZE];
void DebugFreq() {
    char *l_ptrIdx = g_strFreqOutBuffer;
    sprintf(l_ptrIdx, "!%lu", g_intLastPeriod);
    // sprintf(l_ptrIdx, "!%lu<T=%u:H=%u>", g_intLastPeriod, buffer_tail, buffer_head);
    SendString((uint8_t*)(l_ptrIdx), strlen(l_ptrIdx), true);
}

ISR(TIMER_CAPTURE_VECTOR) {
    uint16_t capture_lsw;
    uint32_t l_intCapture, l_intPeriod;
    uint8_t i;

    // get the timer capture
    // return ICR1 (=TCNT1 at ISR moment)
    capture_lsw = capture_read();
    // Handle the case where but capture and overflow interrupts were pending
    // (eg, interrupts were disabled for a while), or where the overflow occurred
    // while this ISR was starting up.  However, if we read a 16 bit number that
    // is very close to overflow, then ignore any overflow since it probably
    // just happened.
    if (capture_overflow() && capture_lsw < 0xFF00) {
        capture_overflow_reset();
        capture_msw++;
    }
    // compute the waveform period
    l_intCapture = ((uint32_t)capture_msw << 16) | capture_lsw;
    l_intPeriod = l_intCapture - capture_previous;
    capture_previous = l_intCapture;

    // g_intLastPeriod = l_intPeriod;
    // SetTask(DebugFreq);
    // if (l_intPeriod < 16000) return; 
    // store it into the buffer
    i = buffer_head + 1;
    if (i >= FREQMEASURE_BUFFER_LEN) i = 0;
    // Если пишем в буфер быстрее, чем читаем - затираем старые данные: нас интересует актуальная частота
    if (i == buffer_tail) {
        buffer_tail++;
        if (buffer_tail >= FREQMEASURE_BUFFER_LEN) buffer_tail = 0;        
    }        
    // if (i != buffer_tail) {
        buffer_value[i] = l_intPeriod;
        buffer_head = i;
    // }
}

CFrequencyMeasure FreqMeasure;

void FreqTimerInit(void) {
    FreqMeasure.begin();
}

#define NO_FREQ 1917.0

float g_fltFrequency = NO_FREQ;

void Capture(void) {
    double l_ftlSum = 0;
    uint32_t l_intCount = 0;
    while (FreqMeasure.available()) {
        l_ftlSum += FreqMeasure.read();                    
        l_intCount++;
    } 
    if (0 != l_intCount)
        g_fltFrequency = FreqMeasure.countToFrequency(l_ftlSum / l_intCount);
    else
        g_fltFrequency = NO_FREQ;
    SetTimerTask(Capture, 200);
}

float GetFrequency() {
    return g_fltFrequency;
}