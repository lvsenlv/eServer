/*************************************************************************
	> File Name: server.h
	> Author: 
	> Mail: 
	> Created Time: 2018年01月29日 星期一 08时55分11秒
 ************************************************************************/

#ifndef __SERVER_H
#define __SERVER_H

#include "common.h"
#include "message.h"
#include "crc.h"
#include <unistd.h>

#define SERVER_ROOT_DIR                     "/root/.eServer"
#define SERVER_USER_LIST_FILE               SERVER_ROOT_DIR"/user.list"
#define SERVER_IDENTITY_FILE_NAME           "identity.info"
#define SERVER_TRANSFER_QUEUE               "/var/eServer_TrsfQ"
#define SERVER_FORMAT_START                 "<START/>\n"
#define SERVER_FORMAT_USER_ID               "    <UserID/>   "
#define SERVER_FORMAT_USER_NAME             "    <UserName/> "
#define SERVER_FORMAT_PASSWORD              "    <Password/> "
#define SERVER_FORMAT_LEVEL                 "    <Level/>    "
#define SERVER_FORMAT_END                   "<END/>\n"

/*
    If the time of twice restarting less than this value,
    it means it quite fast to restart the server task,
    and it may cause unknown error.
    It is recommended that if this situation appeares over 3 times,
    make the process exit.
*/
#define SERVER_TASK_RESTART_TIME_INTERVAL   10

#define SERVER_SOCKET_RECV_BUF_SIZE         (32*1024) //Actual size is (32*1024)*2
#define SERVER_SOCKET_SEND_BUF_SIZE         (32*1024) //Actual size is (32*1024)*2
#define SERVER_USER_MAX_NUM                 512
#define SERVER_SELECT_TIME_INTERVAL         5 //Unit: second
#define SERVER_GET_ACTUAL_USER_ID(id)       (id & (~((uint64_t)0x1 << 63)))

typedef enum {
    SESSION_STATUS_CREATE = 1,
    SESSION_STATUS_ON_LINE,
    SESSION_STATUS_OFF_LINE,
    SESSION_STATUS_BUSY,
}SESSION_STATUS;

typedef struct SessionStruct {
    int fd;
    SESSION_STATUS status;
    char ip[IP_ADDR_MAX_LENGTH];
    uint32_t HoldTime;
    UserInfo_t *pUserInfo;
    struct SessionStruct *pNext;
}session_t;

extern volatile char g_ServerLiveFlag;
extern int g_ServerSocketFd;

G_STATUS SERVER_CreateTask(pthread_t *pServerTaskID);
void *SERVER_ServerTask(void *pArg);



//Static inline functions
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
static inline G_STATUS SERVER_CreateFile(char *pFileName)
{
    FILE *fp = fopen(pFileName, "w+");
    if(NULL == fp)
        return STAT_ERR;

    fclose(fp);
    return STAT_OK;
}

#endif
