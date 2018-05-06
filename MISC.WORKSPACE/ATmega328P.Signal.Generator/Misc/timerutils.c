/*
 * timerutils.c
 *
 * Created: 13.09.2017 10:22:07
 *  Author: Alexey N. Zhukov
 */ 

#include "timerutils.h"

#define PRESCALES 5
bool bestTimer(float theFrequency, uint16_t* theOCR, uint16_t* thePrescaler, float* theResultFrequency) {
	const float l_dblFCPU = (float)(F_CPU);
	const float l_dblPrescales[PRESCALES] = {1, 8, 64, 256, 1024};
	struct {
		bool l_boolOK;
		uint16_t l_intOCR;
		float l_dblError;
	} l_intResOCR[PRESCALES << 1];
	// Немного оптимизируем деление
	float l_dblFrequency = 1 / theFrequency;
	for (uint8_t i = 0 ; i < PRESCALES; i++) {
		float l_dblOCR = l_dblFCPU / (2.0 * theFrequency * l_dblPrescales[i]) - 1.0;
		for (uint8_t j = 0 ; j < 2 ; j++) {
			uint8_t l_intIdx = (i << 1) + j;
			l_intResOCR[l_intIdx].l_boolOK = false;
			float l_dblRoundOCR;
			if (0 == j)
			l_dblRoundOCR = floor(l_dblOCR);
			else
			l_dblRoundOCR = ceil(l_dblOCR);
			if (65535 < l_dblRoundOCR) continue;
			if (0 > l_dblRoundOCR) continue;
			float l_dblFreq = l_dblFCPU / (2.0 * l_dblPrescales[i] * (l_dblRoundOCR + 1.0));
			l_intResOCR[l_intIdx].l_boolOK = true;
			l_intResOCR[l_intIdx].l_intOCR = (uint16_t)l_dblRoundOCR;
			l_intResOCR[l_intIdx].l_dblError = 100.0 * fabs(l_dblFreq - theFrequency) * l_dblFrequency;
		}
	}
	bool l_boolRes = false;
	float l_dblMinError;
	int8_t l_intMinErrorIdx = -1;
	for (uint8_t i = 0 ; i < PRESCALES << 1 ; i++) {
		if (!l_intResOCR[i].l_boolOK) continue;
		l_boolRes = true;
		if ((-1 == l_intMinErrorIdx) || (l_dblMinError >= l_intResOCR[i].l_dblError)) {
			l_intMinErrorIdx = i;
			l_dblMinError = l_intResOCR[i].l_dblError;
		}
	}
	if (l_boolRes) {
		*theOCR = l_intResOCR[l_intMinErrorIdx].l_intOCR;
		*thePrescaler = l_dblPrescales[l_intMinErrorIdx >> 1];
		*theResultFrequency = l_dblFCPU / (2.0 * l_dblPrescales[l_intMinErrorIdx >> 1] * (l_intResOCR[l_intMinErrorIdx].l_intOCR + 1.0));
	}
	return l_boolRes;
}

void initTimer(uint16_t theOCR, uint16_t thePrescaler) {
	TCCR1A = (1 << COM1A0) | (0 << WGM11) | (0 << WGM10);
	switch (thePrescaler) {
		case 1 : {
			TCCR1B = (1 << WGM12) | (0 << CS12) | (0 << CS11) | (1 << CS10);
			break;
		}
		case 8 : {
			TCCR1B = (1 << WGM12) | (0 << CS12) | (1 << CS11) | (0 << CS10);
			break;
		}
		case 64 : {
			TCCR1B = (1 << WGM12) | (0 << CS12) | (1 << CS11) | (1 << CS10);
			break;
		}
		case 256 : {
			TCCR1B = (1 << WGM12) | (1 << CS12) | (0 << CS11) | (0 << CS10);
			break;
		}
		case 1024 : {
			TCCR1B = (1 << WGM12) | (1 << CS12) | (0 << CS11) | (1 << CS10);
			break;
		}
	}
	OCR1A = theOCR;
	TCNT1 = 0;
	
	DDRB |= (1 << DDB1);
}

