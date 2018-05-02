/*************************************************************************
	> File Name: hex_str_to_dec.c
	> Author: 
	> Mail: 
	> Created Time: 2018年04月19日 星期四 08时32分32秒
 ************************************************************************/

#include "hex_str_to_dec.h"

#define ARRAY_SIZE(a)                       (sizeof(a)/sizeof(a[0]))

G_STATUS HexStrTo8Bit(const char *pStr, uint8_t *pRes)
{
    if(('\0' != pStr[0]) && ('0' == pStr[0]) && 
        ('\0' != pStr[1]) && ('x' == pStr[1]))
    {
        pStr += 2; //Ignore delimiter 0x
    }

    uint8_t ResTable[sizeof(uint8_t)*2]; //Two hex symbol occupy 1 byte
    uint8_t res;
    int index;
    int ShiftCount;

    res = 0;
    index = 0;
    ShiftCount = 0;
    
    while(('\0' != *pStr) && ('\n' != *pStr))
    {
        if(('0' <= *pStr) && ('9' >= *pStr))
        {
            ResTable[index++] = (*pStr - '0');
        }
        else if(('a' <= *pStr) && ('f' >= *pStr))
        {
            ResTable[index++] = (*pStr - 'a' + 10);
        }
        else if(('A' <= *pStr) && ('F' >= *pStr))
        {
            ResTable[index++] = (*pStr - 'A' + 10);
        }
        else
            return STAT_ERR;

        pStr++;
        
        if(ARRAY_SIZE(ResTable) == index)
        {
            if(('\0' != *pStr) && ('\n' != *pStr)) //Out of range
                return STAT_ERR;

            break;
        }
    }
    
    if(0 == index)
        return STAT_ERR;

    index--;
    for(; index >= 0; index--)
    {
        res += ResTable[index] << ShiftCount;
        ShiftCount += 4; //Per symbol occupy 4bit binary
    }

    *pRes = res;
    
    return STAT_OK;
}

G_STATUS HexStrTo32Bit(const char *pStr, uint32_t *pRes)
{
    if(('\0' != pStr[0]) && ('0' == pStr[0]) && 
        ('\0' != pStr[1]) && ('x' == pStr[1]))
    {
        pStr += 2; //Ignore delimiter 0x
    }

    uint32_t ResTable[sizeof(uint32_t)*2]; //Two hex symbol occupy 1 byte
    uint32_t res;
    int index;
    int ShiftCount;

    res = 0;
    index = 0;
    ShiftCount = 0;
    
    while(('\0' != *pStr) && ('\n' != *pStr))
    {
        if(('0' <= *pStr) && ('9' >= *pStr))
        {
            ResTable[index++] = (*pStr - '0');
        }
        else if(('a' <= *pStr) && ('f' >= *pStr))
        {
            ResTable[index++] = (*pStr - 'a' + 10);
        }
        else if(('A' <= *pStr) && ('F' >= *pStr))
        {
            ResTable[index++] = (*pStr - 'A' + 10);
        }
        else
            return STAT_ERR;

        pStr++;
        
        if(ARRAY_SIZE(ResTable) == index)
        {
            if(('\0' != *pStr) && ('\n' != *pStr)) //Out of range
                return STAT_ERR;

            break;
        }
    }
    
    if(0 == index)
        return STAT_ERR;

    index--;
    for(; index >= 0; index--)
    {
        res += ResTable[index] << ShiftCount;
        ShiftCount += 4; //Per symbol occupy 4bit binary
    }

    *pRes = res;
    
    return STAT_OK;
}

G_STATUS HexStrTo64Bit(const char *pStr, uint64_t *pRes)
{
    if(('\0' != pStr[0]) && ('0' == pStr[0]) && 
        ('\0' != pStr[1]) && ('x' == pStr[1]))
    {
        pStr += 2; //Ignore delimiter 0x
    }

    uint64_t ResTable[sizeof(uint64_t)*2]; //Two hex symbol occupy 1 byte
    uint64_t res;
    int index;
    int ShiftCount;

    res = 0;
    index = 0;
    ShiftCount = 0;
    
    while(('\0' != *pStr) && ('\n' != *pStr))
    {
        if(('0' <= *pStr) && ('9' >= *pStr))
        {
            ResTable[index++] = (*pStr - '0');
        }
        else if(('a' <= *pStr) && ('f' >= *pStr))
        {
            ResTable[index++] = (*pStr - 'a' + 10);
        }
        else if(('A' <= *pStr) && ('F' >= *pStr))
        {
            ResTable[index++] = (*pStr - 'A' + 10);
        }
        else
            return STAT_ERR;

        pStr++;
        
        if(ARRAY_SIZE(ResTable) == index)
        {
            if(('\0' != *pStr) && ('\n' != *pStr)) //Out of range
                return STAT_ERR;

            break;
        }
    }
    
    if(0 == index)
        return STAT_ERR;

    index--;
    for(; index >= 0; index--)
    {
        res += ResTable[index] << ShiftCount;
        ShiftCount += 4; //Per symbol occupy 4bit binary
    }

    *pRes = res;
    
    return STAT_OK;
}
