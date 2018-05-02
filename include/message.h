/*************************************************************************
	> File Name: message.h
	> Author: 
	> Mail: 
	> Created Time: 2018年01月31日 星期三 16时35分36秒
 ************************************************************************/

#ifndef __MESSAGE_H
#define __MESSAGE_H

#include "common.h"
#include "completion_code.h"

#define MSG_QUEUE_NAME                      "/var/eServer_MsgQ"
#define HANDSHAKE_SYMBOL                    '#'

/*
    If the time of twice restarting less than this value,
    it means it quite fast to restart the msg task,
    and it may cause unknown error.
    It is recommended that if this situation appeares over 3 times,
    make the process exit.
*/
#define MSG_TASK_RESTART_TIME_INTERVAL      10

#define MSG_SELECT_TIME_INTERVAL            5 //Unit: second
#define MSG_PALYPOAD_CRC_SIZE               (sizeof(MsgPkt_t)-sizeof(uint16_t)-MSG_DATA_MAX_LENGTH)
#define MSG_PALYPOAD_SIZE                   (sizeof(MsgPkt_t)-MSG_DATA_MAX_LENGTH)
#define MSG_DATA_MAX_LENGTH                 4096

#define MSG_CLR_RES(f)                      (f &= (~0x0001))
#define MSG_SET_RES(f)                      (f |= 0x0001)
#define MSG_IF_RES(f)                       (f & 0x0001)

typedef struct MsgPktStruct {
    uint8_t NetFn;
    uint8_t cmd;
    int fd;
    uint32_t DataSize;
    uint32_t DataCRC;
    uint16_t flag;
    
    uint16_t PayloadCRC;
    uint8_t data[MSG_DATA_MAX_LENGTH];
}PACKED MsgPkt_t; //If not "PACKED", it may cause CRC verify error

typedef struct MsgPktTransferDataStruct {
    uint8_t NetFn;
    uint8_t cmd;
    FILE *fp;
    uint64_t offset;
    int size;
    uint16_t PayloadCRC;
}PACKED MsgPktTransferData_t; //If not "PACKED", it may cause CRC verify error

typedef G_STATUS (*MsgCmdHandleFunc) (MsgPkt_t *);

typedef struct CmdTableStruct {
    uint8_t cmd;
    MsgCmdHandleFunc MsgCmdHandle;
}CmdTable_t;

typedef struct NetFnTableStruct {
    uint8_t NetFn;
    const CmdTable_t *CmdTable;
}NetFnTable_t;

typedef enum {
    MSG_NET_FN_COMMON                       = 0x03,
}MSG_NET_FN;

typedef enum {
    MSG_CMD_SEND_RES                        = 0x01,
    MSG_CMD_DO_NOTHING                      = 0xFE,
}MSG_CMD_COMMON;

typedef struct UserInfoStruct {
    uint64_t UserID;
    char UserName[USER_NAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
    uint8_t level;
    struct UserInfoStruct *pNext;
}UserInfo_t;

typedef struct MsgDataVerifyIdentityStruct {
    char UserName[USER_NAME_MAX_LENGTH];
    char password[PASSWORD_MAX_LENGTH];
}MsgDataVerifyIdentity_t;

typedef struct MsgDataResStruct {
    uint16_t CC;
}MsgDataRes_t;

typedef struct MsgDataAddUserStruct {
    UserInfo_t CurUserInfo;
    UserInfo_t NewUserInfo;
}MsgDataAddUser_t;

extern char g_MsgTaskLiveFlag;

G_STATUS MSG_CreateTask(pthread_t *pMsgTaskID);
void *MSG_MessageTask(void *pArg);
void MSG_InitMsgPkt(MsgPkt_t *pMsgPkt);
G_STATUS MSG_BeforeCreateTask(void);
G_STATUS MSG_LockMsgQueue(int fd);
G_STATUS MSG_UnlockMsgQueue(int fd);
G_STATUS MSG_PostMsg(MsgPkt_t *pMsgPkt);
G_STATUS MSG_PostMsgNoLock(MsgPkt_t *pMsgPkt);
G_STATUS MSG_GetResponse(int fd, void *pBuf, int BufSize, int timeout);

#endif
