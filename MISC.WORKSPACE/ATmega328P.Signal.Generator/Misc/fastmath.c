/*
 * math.c
 *
 * Created: 06.09.2017 17:35:33
 *  Author: Alexey N. Zhukov
 */ 

#include "fastmath.h"

uint8_t div3u8(uint8_t theValue) {
	uint8_t l_intRes = (theValue >> 2) + (theValue >> 4);
	l_intRes += l_intRes >> 4;
	uint8_t l_intR = theValue - l_intRes * 3;
	return l_intRes + (11 * l_intR >> 5);
}

uint16_t div3u16(uint16_t theValue) {
	uint16_t l_intRes = (theValue >> 2) + (theValue >> 4);
	l_intRes += l_intRes >> 4;
	l_intRes += l_intRes >> 8;
	uint16_t l_intR = theValue - l_intRes * 3;
	return l_intRes + (11 * l_intR >> 5);
}

uint32_t div3u32(uint32_t theValue) {
	uint32_t l_intRes = (theValue >> 2) + (theValue >> 4);
	l_intRes += l_intRes >> 4;
	l_intRes += l_intRes >> 8;
	l_intRes += l_intRes >> 16;
	uint32_t l_intR = theValue - l_intRes * 3;
	return l_intRes + (11 * l_intR >> 5);
}
