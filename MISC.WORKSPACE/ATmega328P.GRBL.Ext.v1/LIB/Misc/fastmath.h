/*
 * math.h
 *
 * Created: 06.09.2017 17:32:15
 *  Author: Alexey N. Zhukov
 */ 


#ifndef FASTMATH_H_
#define FASTMATH_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t div3u8(uint8_t theValue);
uint16_t div3u16(uint16_t theValue);
uint32_t div3u32(uint32_t theValue);

#ifdef __cplusplus
}
#endif

#endif /* FASTMATH_H_ */