/*************************************************************************
	> File Name: message.c
	> Author: 
	> Mail: 
	> Created Time: 2018年01月31日 星期三 16时35分24秒
 ************************************************************************/

#ifdef __SERVER
#include "server.h"
#else
#include "message.h"
#endif
#include "log.h"
#include "crc.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

static G_STATUS MSG_CreateMsgQueue(void);
static G_STATUS MSG_InitGlobalVariables(void);
static G_STATUS MSG_GetCmdHandle(MsgPkt_t *pMsgPkt, MsgCmdHandleFunc *pMsgCmdHandle);
static G_STATUS MSG_ProcessMsg(MsgPkt_t *pMsgPkt);

const CmdTable_t MSG_RootCmdTable[] = 
{
#ifdef __SERVER
#endif
    {0xFF,                              NULL},
};

const CmdTable_t MSG_AdminCmdTable[] = 
{
#ifdef __SERVER
#endif
    {0xFF,                              NULL},
};

const CmdTable_t MSG_CommonCmdTable[] = 
{
#ifdef __SERVER
#endif
    {0xFF,                              NULL},
};

const NetFnTable_t MSG_NetFnTable[] = 
{
    {0xFF,                              NULL},
};

int g_MsgQueue;
char g_MsgTaskLiveFlag;

G_STATUS MSG_CreateTask(pthread_t *pMsgTaskID)
{
    int res;
    pthread_t MsgTaskID;
    pthread_attr_t MsgTaskAttr;
    
    if(STAT_OK != MSG_BeforeCreateTask())
        return STAT_ERR;
    
    res = pthread_attr_init(&MsgTaskAttr);
    if(0 != res)
    {
        LOG_FATAL_ERROR("[MSG create task] Fail to init thread attribute\n");
        return STAT_ERR;
    }
    
    res = pthread_attr_setdetachstate(&MsgTaskAttr, PTHREAD_CREATE_DETACHED);
    if(0 != res)
    {
        LOG_FATAL_ERROR("[MSG create task] Fail to set thread attribute\n");
        return STAT_ERR;
    }

    res = pthread_create(&MsgTaskID, &MsgTaskAttr, MSG_MessageTask, NULL);
    if(0 != res)
    {
        LOG_FATAL_ERROR("[MSG create task] Fail to create task\n");
        return STAT_ERR;
    }
    
    pthread_attr_destroy(&MsgTaskAttr);
    *pMsgTaskID = MsgTaskID;

    return STAT_OK;
}

void *MSG_MessageTask(void *pArg)
{
    struct timeval TimeInterval;
    fd_set ReadFds;
    int res;
    int ReadDataLength;
    MsgPkt_t MsgPkt;
    uint32_t crc;

    TimeInterval.tv_usec = 0;
    
    while(1)
    {
        g_MsgTaskLiveFlag = 1;
        
        FD_ZERO(&ReadFds);
        FD_SET(g_MsgQueue, &ReadFds);
        TimeInterval.tv_sec = MSG_SELECT_TIME_INTERVAL;
        
        res = select(g_MsgQueue+1, &ReadFds, NULL, NULL, &TimeInterval);
        if(0 > res)
        {
            LOG_ERROR("[Msg task] Select function return negative value\n");
            continue;
        }
        else if(0 == res)
            continue;
        
        if(!(FD_ISSET(g_MsgQueue, &ReadFds)))
            continue;

        ReadDataLength = read(g_MsgQueue, &MsgPkt, sizeof(MsgPkt_t));
        if(sizeof(MsgPkt_t) != ReadDataLength)
        {
            LOG_ERROR("[Msg task] read(): %s\n", strerror(errno));
            continue;
        }

        crc = CRC16_calculate(&MsgPkt, MSG_PALYPOAD_CRC_SIZE);
        if(crc != MsgPkt.PayloadCRC)
        {
            LOG_ERROR("[Msg task] Payload CRC verify error\n");
            continue;
        }

        if(MSG_DATA_MAX_LENGTH < MsgPkt.DataSize)
        {
            LOG_ERROR("[Msg task] Msg data size is too long\n");
            continue;
        }

        crc = CRC32_calculate(&MsgPkt.data, MsgPkt.DataSize);
        if(crc != MsgPkt.DataCRC)
        {
            LOG_ERROR("[Msg task] Data CRC verify error\n");
            continue;
        }

        MSG_ProcessMsg(&MsgPkt);
    }

    return NULL;
}





#define COMMON_FUNC //Only use for locating function efficiently
//Common function
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
G_STATUS MSG_BeforeCreateTask(void)
{
    if(STAT_OK != MSG_InitGlobalVariables())
        return STAT_ERR;
        
    if(STAT_OK != MSG_CreateMsgQueue())
        return STAT_ERR;

#ifdef __CRC8
    CRC8_InitTable();
#endif

#ifdef __CRC16
    CRC16_InitTable();
#endif

#ifdef __CRC32
    CRC32_InitTable();
#endif

    return STAT_OK;
}

void MSG_InitMsgPkt(MsgPkt_t *pMsgPkt)
{
    pMsgPkt->NetFn = MSG_NET_FN_COMMON;
    pMsgPkt->cmd = MSG_CMD_DO_NOTHING;
    pMsgPkt->fd = -1;
    pMsgPkt->DataSize = 0;
    pMsgPkt->DataCRC = 0;
    pMsgPkt->flag = 0;
    pMsgPkt->PayloadCRC = 0;
    memset(pMsgPkt->data, 0, sizeof(pMsgPkt->data));
}

G_STATUS MSG_LockMsgQueue(int fd)
{
    struct flock lock;

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = (pid_t)0;
    
    if(-1 == fcntl(fd, F_SETLKW, &lock))
        return STAT_ERR;

    return STAT_OK;
}

G_STATUS MSG_UnlockMsgQueue(int fd)
{
    struct flock lock;

    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = (pid_t)0;
    
    if(-1 == fcntl(fd, F_SETLK, &lock))
        return STAT_ERR;

    return STAT_OK;
}

G_STATUS MSG_PostMsg(MsgPkt_t *pMsgPkt)
{
    if(STAT_OK != MSG_LockMsgQueue(g_MsgQueue))
    {
        LOG_ERROR("[Post msg] Fail to lock msg queue\n");
        return STAT_ERR;
    }

    int WriteDataLength;
    WriteDataLength = write(g_MsgQueue, pMsgPkt, sizeof(MsgPkt_t));
    if(sizeof(MsgPkt_t) != WriteDataLength)
    {
        MSG_UnlockMsgQueue(g_MsgQueue);
        LOG_ERROR("[Post msg] write(): %s\n", strerror(errno));
        return STAT_ERR;
    }

    if(STAT_OK != MSG_UnlockMsgQueue(g_MsgQueue))
    {
        LOG_ERROR("[Post msg] Fail to unlock msg queue\n");
        return STAT_ERR;
    }

    return STAT_OK;
}

G_STATUS MSG_PostMsgNoLock(MsgPkt_t *pMsgPkt)
{
    int WriteDataLength;
    
    WriteDataLength = write(g_MsgQueue, pMsgPkt, sizeof(MsgPkt_t));
    if(sizeof(MsgPkt_t) != WriteDataLength)
    {
        LOG_ERROR("[Post msg] write(): %s\n", strerror(errno));
        return STAT_ERR;
    }

    return STAT_OK;
}

G_STATUS MSG_GetResponse(int fd, void *pBuf, int size, int timeout)
{
    struct timeval TimeInterval;
    fd_set fds;
    int res;
    int ReadDataLength;

    TimeInterval.tv_usec = 0;

    do
    {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        TimeInterval.tv_sec = timeout;

        res = select(fd+1, &fds, NULL, NULL, &TimeInterval);
        if(0 > res)
            break;

        if(0 == res)
            return STAT_ERR;
            
        if(!(FD_ISSET(fd, &fds)))
            return STAT_ERR;

        ReadDataLength = read(fd, pBuf, size);
        if(size != ReadDataLength)
            return STAT_ERR;

        break;
    }while(0);
    
    return STAT_OK;
}
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//Common function





#define STATIC_FUNC //Only use for locating function efficiently
//Static function
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

static G_STATUS MSG_CreateMsgQueue(void)
{
    if(0 == access(MSG_QUEUE_NAME, F_OK))
    {
        if(0 != unlink(MSG_QUEUE_NAME))
        {
            LOG_FATAL_ERROR("[MSG create queue] unlink(): %s", strerror(errno));
            return STAT_FATAL_ERR;
        }
    }
    
    if(0 != mkfifo(MSG_QUEUE_NAME, 0600))
    {
        LOG_FATAL_ERROR("[MSG create queue] mkfifo(): %s", strerror(errno));
        return STAT_FATAL_ERR;
    }
    
    g_MsgQueue = open(MSG_QUEUE_NAME, O_RDWR); //It would be block if not use O_RDWR
    if(0 > g_MsgQueue)
    {
        LOG_FATAL_ERROR("[MSG create queue] open(): %s", strerror(errno));
        return STAT_FATAL_ERR;
    }
    
    return STAT_OK;
}

static G_STATUS MSG_InitGlobalVariables(void)
{
    g_MsgTaskLiveFlag = 0;
    
    return STAT_OK;
}

static G_STATUS MSG_GetCmdHandle(MsgPkt_t *pMsgPkt, MsgCmdHandleFunc *ppMsgCmdHandle)
{
    uint8_t i;
    const CmdTable_t *CmdTable;

    for(i = 0x00; i < 0xFF; i++)
    {
        if(MSG_NetFnTable[i].NetFn == pMsgPkt->NetFn)
            break;

        if(0xFF == pMsgPkt->NetFn)
        {
            i = 0xFF;
            break;
        }
    }

    if(0xFF <= i)
        return STAT_ERR;

    CmdTable = MSG_NetFnTable[i].CmdTable;

    for(i = 0x00; i < 0xFF; i++)
    {
        if(CmdTable[i].cmd == pMsgPkt->cmd)
            break;

        if(0xFF == CmdTable[i].cmd)
        {
            i = 0xFF;
            break;
        }
    }

    if(0xFF <= i)
        return STAT_ERR;

    *ppMsgCmdHandle = CmdTable[i].MsgCmdHandle;

    return STAT_OK;
}

static G_STATUS MSG_ProcessMsg(MsgPkt_t *pMsgPkt)
{
    MsgCmdHandleFunc pMsgCmdHandle;
    G_STATUS status;

    if(STAT_OK != MSG_GetCmdHandle(pMsgPkt, &pMsgCmdHandle))
    {
        LOG_DEBUG("[MSG process msg] Invalid NetFn or cmd\n");
        return STAT_ERR;
    }

    //LOG_DEBUG("[MSG process msg] NetFn=0x%x, cmd=0x%x\n", pMsgPkt->NetFn, pMsgPkt->cmd);
    status = (*pMsgCmdHandle)(pMsgPkt);
    
    return status;
}
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//Static function
