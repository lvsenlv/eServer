/*************************************************************************
	> File Name: completion_code.h
	> Author: 
	> Mail: 
	> Created Time: 2018年02月07日 星期三 10时42分07秒
 ************************************************************************/

#ifndef __CC_H
#define __CC_H

typedef enum {
    CC_NORMAL                               = 0x00,

    CC_CRC16_ERROR,
    CC_CRC32_ERROR,
    
    CC_FAIL_TO_CLOSE_SESSION,
    CC_FAIL_TO_GET_FILE_INFO,
    CC_FAIL_TO_MALLOC,
    CC_FAIL_TO_MK_DIR,
    CC_FAIL_TO_OPEN,
    CC_FAIL_TO_READ,
    CC_FAIL_TO_SEEK,
    CC_FAIL_TO_UNLINK,
    CC_FAIL_TO_WRITE,
    CC_FAIL_TO_RM_DIR,
    CC_FAIL_TO_RN_DIR,
    
    CC_INVALID_CMD,
    CC_INVALID_FD,
    CC_INVALID_FP,
    CC_INVALID_NET_FN,
    CC_INVALID_OFFSET,
    CC_INVALID_OFFSET_OR_SIZE,
    CC_INVALID_SIZE,
    
    CC_EMPTY_FILE,
    CC_FILE_DOES_NOT_EXIST,
    CC_FILE_IS_TOO_BIG,
    CC_PASSWORD_IS_TOO_SHORT,
    CC_PERMISSION_DENIED,
    CC_SESSION_IS_NOT_FOUND,
    CC_USER_HAS_BEEN_EXISTED,
    CC_USER_DOES_NOT_EXIST,
    CC_USER_LIST_IS_FULL,
    
    CC_MAX,
}COMPLETION_CODE;

extern char *g_CompletionCodeTable[CC_MAX];

void InitErrorCodeTable(void);



static inline char *GetErrorDetails(COMPLETION_CODE code)
{
    if((0 >= code) || (CC_MAX <= code))
        return "Unknown error";
    
    return g_CompletionCodeTable[code];
}

#endif
