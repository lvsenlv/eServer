/*************************************************************************
	> File Name: hex_str_to_dec.h
	> Author: 
	> Mail: 
	> Created Time: 2018年04月19日 星期四 10时11分13秒
 ************************************************************************/

#ifndef __HEX_STR_TO_DEC_H
#define __HEX_STR_TO_DEC_H

#include "common.h"

G_STATUS HexStrTo8Bit(const char *pStr, uint8_t *pRes);
G_STATUS HexStrTo32Bit(const char *pStr, uint32_t *pRes);
G_STATUS HexStrTo64Bit(const char *pStr, uint64_t *pRes);

#endif
