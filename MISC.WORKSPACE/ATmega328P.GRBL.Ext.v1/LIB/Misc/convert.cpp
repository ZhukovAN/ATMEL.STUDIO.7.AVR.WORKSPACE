/*
 * convert.c
 *
 * Created: 17.09.2017 13:06:47
 *  Author: Alexey N. Zhukov
 */ 

#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>
#include "convert.h"

typedef struct {
    uint32_t quot;
    uint8_t rem;
} divmod10_t;

inline static divmod10_t divmodu10(uint32_t n) {
    divmod10_t l_intRes;
    // умножаем на 0.8
    l_intRes.quot = n >> 1;
    l_intRes.quot += l_intRes.quot >> 1;
    l_intRes.quot += l_intRes.quot >> 4;
    l_intRes.quot += l_intRes.quot >> 8;
    l_intRes.quot += l_intRes.quot >> 16;
    uint32_t qq = l_intRes.quot;
    // делим на 8
    l_intRes.quot >>= 3;
    // вычисляем остаток
    l_intRes.rem = (uint8_t)(n - ((l_intRes.quot << 1) + (qq & ~7ul)));
    // корректируем остаток и частное
    if(l_intRes.rem > 9) {
        l_intRes.rem -= 10;
        l_intRes.quot++;
    }
    return l_intRes;
}

char* utoa_fast_div(uint32_t theValue, char* theBuffer) {
    theBuffer += 11;
    *--theBuffer = 0;
    do {
        divmod10_t l_intRes = divmodu10(theValue);
        *--theBuffer = l_intRes.rem + '0';
        theValue = l_intRes.quot;
    } while (0 != theValue);
    return theBuffer;
}

const uint32_t g_intTable[] PROGMEM = {
    0xF0BDC21A,
    0x3DA137D5,
    0x9DC5ADA8,
    0x2863C1F5,
    0x6765C793,
    0x1A784379,
    0x43C33C19,
    0xAD78EBC5,
    0x2C68AF0B,
    0x71AFD498,
    0x1D1A94A2,
    0x4A817C80,
    0xBEBC2000,
    0x30D40000,
    0x7D000000,
    0x20000000,
    0x51EB851E,
    0xD1B71758,
    0x35AFE535,
    0x89705F41,
    0x232F3302,
    0x5A126E1A,
    0xE69594BE,
    0x3B07929F,
    0x971DA050,
    0x26AF8533,
    0x63090312,
    0xFD87B5F2,
    0x40E75996,
    0xA6274BBD,
    0x2A890926,
    0x6CE3EE76
};

static inline uint32_t mul32hu(uint32_t u, uint32_t v) {
#if defined(AVR) || defined (__AVR__)
    uint8_t a0 = (uint8_t)(u >> 0);
    uint8_t a1 = (uint8_t)(u >> 8);
    uint8_t a2 = (uint8_t)(u >> 16);
    uint8_t a3 = (uint8_t)(u >> 24);

    uint8_t b0 = (uint8_t)(v >> 0);
    uint8_t b1 = (uint8_t)(v >> 8);
    uint8_t b2 = (uint8_t)(v >> 16);
    uint8_t b3 = (uint8_t)(v >> 24);
    
    union {
        struct {
            uint8_t w0, w1, w2, w3, w4, w5, w6, w7;
        } u8;
        uint32_t u32[2];
    };
    
    u8.w0 = u8.w1 = u8.w2 = u8.w3 = u8.w4 = u8.w5 = u8.w6 = u8.w7 = 0;

    uint8_t k = 0;
    uint16_t t;

    t = a0 * b0 + u8.w0 + k;	u8.w0 = t;	k = t >> 8;
    t = a1 * b0 + u8.w1 + k;	u8.w1 = t;	k = t >> 8;
    t = a2 * b0 + u8.w2 + k;	u8.w2 = t;	k = t >> 8;
    t = a3 * b0 + u8.w3 + k;	u8.w3 = t;	k = t >> 8;
    u8.w4 = k;	k = 0;

    t = a0 * b1 + u8.w1 + k;	u8.w1 = t;	k = t >> 8;
    t = a1 * b1 + u8.w2 + k;	u8.w2 = t;	k = t >> 8;
    t = a2 * b1 + u8.w3 + k;	u8.w3 = t;	k = t >> 8;
    t = a3 * b1 + u8.w4 + k;	u8.w4 = t;	k = t >> 8;
    u8.w5 = k;	k = 0;

    t = a0 * b2 + u8.w2 + k;	u8.w2 = t;	k = t >> 8;
    t = a1 * b2 + u8.w3 + k;	u8.w3 = t;	k = t >> 8;
    t = a2 * b2 + u8.w4 + k;	u8.w4 = t;	k = t >> 8;
    t = a3 * b2 + u8.w5 + k;	u8.w5 = t;	k = t >> 8;
    u8.w6 = k;	k = 0;

    t = a0 * b3 + u8.w3 + k;	u8.w3 = t;	k = t >> 8;
    t = a1 * b3 + u8.w4 + k;	u8.w4 = t;	k = t >> 8;
    t = a2 * b3 + u8.w5 + k;	u8.w5 = t;	k = t >> 8;
    t = a3 * b3 + u8.w6 + k;	u8.w6 = t;	k = t >> 8;
    u8.w7 = k;
    
    return u32[1];
#else
    return uint32_t((uint64_t(u) * v) >> 32);
#endif
}

int ftoaEngine(float theValue, char *theBuffer, int l_intPrecision) {
    // Нам надо побитно разобрать float-значение
    uint32_t l_intValue = *reinterpret_cast<uint32_t*>(&theValue);
    uint16_t l_intValueHi16 = (uint16_t)(l_intValue >> 16);
    uint8_t l_intExponent = (uint8_t(l_intValueHi16 >> 7));
    uint32_t l_intFraction = (l_intValue & 0x00FFFFFF) | 0x00800000;
    char *l_ptrBuffer = theBuffer;

    if (l_intValue & 0x80000000)
        *l_ptrBuffer++ = '-';
    else
        *l_ptrBuffer++ = '+';

    if (l_intExponent == 0) {
        // don't care about a subnormals
        l_ptrBuffer[0] = '0';
        l_ptrBuffer[1] = 0;
        return 0xFF;
    }

    if (l_intExponent == 0xFF) {
        if (l_intFraction & 0x007FFFFF) {
            l_ptrBuffer[0] = 'n';
            l_ptrBuffer[1] = 'a';
            l_ptrBuffer[2] = 'n';
            l_ptrBuffer[3] = 0;
        } else {
            l_ptrBuffer[0] = 'i';
            l_ptrBuffer[1] = 'n';
            l_ptrBuffer[2] = 'f';
            l_ptrBuffer[3] = 0;
        }
        return 0xFF;
    }

    *l_ptrBuffer++ = '0';

    int l_intExp10 = ((((l_intExponent >> 3)) * 77 + 63) >> 5) - 38;
    l_intFraction <<= 8;
    uint32_t l_intTemp = mul32hu(l_intFraction, pgm_read_dword(&g_intTable[l_intExponent / 8])) + 1;
    uint_fast8_t l_intShift = 7 - (l_intExponent & 7);
    l_intTemp >>= l_intShift;
    // Теперь удалим ведущие нули
    uint8_t l_intDigit = l_intTemp >> 24;
    l_intDigit >>= 4;
    while (l_intDigit == 0) {
        l_intTemp &= 0x0FFFFFFF;
        l_intTemp <<= 1;
        l_intTemp += l_intTemp << 2;
        l_intDigit = (uint8_t)(l_intTemp >> 24);
        l_intDigit >>= 4;
        l_intExp10--;
    }
    // Извлечение цифр
    for(uint_fast8_t i = l_intPrecision + 1; i > 0; i--) {
        l_intDigit = (uint8_t)(l_intTemp >> 24);
        l_intDigit >>= 4;
        *l_ptrBuffer++ = l_intDigit + '0';
        l_intTemp &= 0x0FFFFFFF;
        l_intTemp <<= 1;
        l_intTemp += l_intTemp << 2;
    }
    // Округление
    if (theBuffer[l_intPrecision + 2] >= '5')
        theBuffer[l_intPrecision + 1]++;
    l_ptrBuffer[-1] = 0;
    l_ptrBuffer -= 2;
    for(uint_fast8_t i = l_intPrecision + 1; i > 1; i--) {
        if (theBuffer[i] > '9') {
            theBuffer[i] -= 10;
            theBuffer[i - 1]++;
        }
    }
    while (*l_ptrBuffer == '0')
        *l_ptrBuffer-- = 0;
    return l_intExp10;
}
        

char * my_ftoa(float theValue, char *theResult) {
    uint8_t l_intPrecision = 8;
    char *l_ptrRes = theResult;
    const int l_intBufferSize = 12;
    char l_intBuffer[l_intBufferSize + 1];
    // получили цифры
    int l_intExp10 = ftoaEngine(theValue, l_intBuffer, l_intPrecision);
    // если там inf или nan - выводим как есть.
    if (l_intExp10 == 0xFF) {
        uint8_t l_intDigits = strlen(l_intBuffer);
        uint_fast8_t i = 0;
        if (l_intBuffer[0] == '+')
            i = 1;
        for (; i < l_intDigits; i++)
        *l_ptrRes++ = l_intBuffer[i];
        *l_ptrRes = 0;
        return theResult;
    }
    // если был перенос старшей цифры при округлении
    char *l_ptrBegin = &l_intBuffer[2];
    if (l_intBuffer[1] != '0') {
        l_intExp10++;
        l_ptrBegin--;
    }
    // количество значащих цифр <= precision
    uint_fast8_t l_intDigits = strlen(l_ptrBegin);
    
    uint_fast8_t l_intIntDigits = 0, l_intLeadingZeros = 0;
    if (abs(l_intExp10) >= l_intPrecision)
        l_intIntDigits = 1;
    else if(l_intExp10 >= 0) {
        l_intIntDigits = l_intExp10 + 1;
        l_intExp10 = 0;
    } else {
        l_intIntDigits = 0;
        l_intLeadingZeros = -l_intExp10 - 1;
        l_intExp10 = 0;
    }
    uint_fast8_t fractDigits = l_intDigits > l_intIntDigits ? l_intDigits - l_intIntDigits : 0;
    // целая часть
    if (l_intIntDigits) {
        uint_fast8_t l_intCount = l_intIntDigits > l_intDigits ? l_intDigits : l_intIntDigits;
        while (l_intCount--)
            *l_ptrRes++ = *l_ptrBegin++;
        int_fast8_t tralingZeros = l_intIntDigits - l_intDigits;
        while (tralingZeros-- > 0)
            *l_ptrRes++ ='0';
    } else
        *l_ptrRes++ = '0';
    // дробная часть
    if (fractDigits) {
        *l_ptrRes++ = '.';
        while(l_intLeadingZeros--)
            *l_ptrRes++ = '0';
        while(fractDigits--)
            *l_ptrRes++ = *l_ptrBegin++;
    }
    // десятичная экспонента
    if (l_intExp10 != 0) {
        *l_ptrRes++ = 'e';
        uint_fast8_t l_intPow10;
        if(l_intExp10 < 0) {
            *l_ptrRes++ = '-';
            l_intPow10 = -l_intExp10;
        } else {
            *l_ptrRes++ = '+';
            l_intPow10 = l_intExp10;
        }
        char l_ptrPowBuffer[11];
        char *l_ptrPow = utoa_fast_div(l_intPow10, l_ptrPowBuffer);
        while (*l_ptrPow != 0)
            *l_ptrRes++ = *l_ptrPow++;
    }
    *l_ptrRes = 0;
    return theResult;
}