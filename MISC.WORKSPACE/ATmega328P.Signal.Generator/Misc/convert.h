/*
 * convert.h
 *
 * Created: 17.09.2017 13:06:32
 *  Author: Alexey N. Zhukov
 */ 


#ifndef CONVERT_H_
#define CONVERT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
    
char* utoa_fast_div(uint32_t theValue, char* theBuffer);
char * my_ftoa(float value, char *result);

#ifdef __cplusplus
}
#endif

#endif /* CONVERT_H_ */