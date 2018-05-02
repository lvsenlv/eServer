/*************************************************************************
	> File Name: server.c
	> Author: 
	> Mail: 
	> Created Time: 2018年01月30日 星期二 08时39分23秒
 ************************************************************************/

#include "server.h"
#include "log.h"
#include "completion_code.h"
#include "hex_str_to_dec.h"
#include <signal.h>
#include <fcntl.h>
#include <sys/param.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <errno.h>

volatile char g_ServerLiveFlag;         //1 mean server task is alive
int g_ServerSocketFd;                   //It would be initialized in server task
int g_ServerTransferFd;

/*
    Mutex lock sequence:
        g_SessionLock -> g_LogLockTbl[*]
    Warning:
        strictly ban using g_LogLockTbl[*] before using g_SessionLock
*/
UserInfo_t g_HeadUserInfo;

pthread_mutex_t g_SessionLock = PTHREAD_MUTEX_INITIALIZER;
__IO int g_MaxFd;                       //It would be initialized in server task
__IO fd_set g_PrevFds;                  //It would be initialized in server task
session_t g_HeadSession;
session_t *g_pTailSession;

#define STATIC_FUNC //Only use for locating function efficiently
//Static function
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

/*
 *  @Briefs: Create related directory
 *  @Return: STAT_FATAL_ERR / STAT_OK
 *  @Note: None
 */
static G_STATUS SERVER_InitServerFile(void)
{
    if(0 != access(SERVER_ROOT_DIR, F_OK))
    {
        if(0 != mkdir(SERVER_ROOT_DIR, S_IRUSR | S_IWUSR))
        {
            LOG_FATAL_ERROR("[S* Init server file] mkdir(%s): %s\n", SERVER_ROOT_DIR, strerror(errno));
            return STAT_FATAL_ERR;
        }
    }
    
    return STAT_OK;
}

/*
 *  @Briefs: Initialize global varibles
 *  @Return: Always STAT_OK
 *  @Note: None
 */
static G_STATUS SERVER_InitGlobalVaribles(void)
{
    g_ServerLiveFlag = 0;
    g_ServerSocketFd = 0;
    g_ServerTransferFd = 0;
    g_pTailSession = &g_HeadSession;
    memset(&g_HeadSession, 0, sizeof(session_t));
    memset(&g_HeadUserInfo, 0, sizeof(UserInfo_t));
    
    return STAT_OK;
}

static G_STATUS SERVER_ParseUserListFile(void)
{
    return STAT_OK;
}

/*
 *  @Briefs: Create user id based on system time
 *  @Return: User id
 *  @Note: None
 */
static uint64_t SERVER_CreateUserID(void)
{
    uint64_t UserID;
    struct timeval TimeInterval;
    
    gettimeofday(&TimeInterval, NULL);
    UserID = TimeInterval.tv_sec;

    return UserID;
}

/*
 *  @Briefs: Get the content in user.list and set in g_HeadUserInfo
 *  @Return: STAT_OK / STAT_FATAL_ERR
 *  @Note:   Format: 
 *           <START/>
 *               <UserID/>   ...
 *               <UserName/> ...
 *               <Password/> ...
 *               <Level/>    ...
 *           <END/>
 */
static G_STATUS SERVER_LoadUserTable(void)
{
    FILE *fp;
    char buf[SMALL_BUF_SIZE];
    UserInfo_t *pCurUserInfo;
    UserInfo_t *pNewUserInfo;
    int i;
    uint64_t UserID;
    char *pTmp;
    char *pTmp2;
    int LineCount;

    LineCount = 0;
    
    if(0 != access(SERVER_USER_LIST_FILE, F_OK))
    {
        LOG_INFO("[S* Load user] No such file: %s\n[S* load user table] Create file: %s\n", 
            SERVER_USER_LIST_FILE, SERVER_USER_LIST_FILE);
            
        if(STAT_OK != CreateFile(SERVER_USER_LIST_FILE, 0600))
        {
            LOG_FATAL_ERROR("[S* Load user] Fail to reate (%s): %s\n", 
                SERVER_USER_LIST_FILE, strerror(errno));
            return STAT_FATAL_ERR;
        }
        
        return STAT_OK;
    }
    
    g_HeadUserInfo.UserID = 4884;
    g_HeadUserInfo.pNext = NULL;
    memcpy(g_HeadUserInfo.UserName, "root", sizeof("root"));
    memcpy(g_HeadUserInfo.password, "root@eServer", sizeof("root@eServer"));
    pCurUserInfo = &g_HeadUserInfo;
    
    fp = fopen(SERVER_USER_LIST_FILE, "rb");
    if(NULL == fp)
    {
        LOG_FATAL_ERROR("[S* Load user] fopen(%s): %s\n", SERVER_USER_LIST_FILE, strerror(errno));
        return STAT_FATAL_ERR;
    }

    while(1)
    {
        if(NULL == fgets(buf, sizeof(buf), fp) || (0 != feof(fp)))
            break;

        LineCount++;

        if(0 != strcmp(SERVER_FORMAT_START, buf))
        {
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }
        
        pNewUserInfo = (UserInfo_t *)malloc(sizeof(UserInfo_t));
        if(NULL == pNewUserInfo)
        {
            fclose(fp);
            LOG_ERROR("[S* load user] malloc(): %s\n", strerror(errno));
            return STAT_ERR;
        }

        //Get UserID >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
        if(NULL == fgets(buf, sizeof(buf), fp) || (0 != feof(fp)))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }

        LineCount++;

        if(0 != strncmp(SERVER_FORMAT_USER_ID, buf, sizeof(SERVER_FORMAT_USER_ID)-1))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }

        if(STAT_OK != HexStrTo64Bit(buf+sizeof(SERVER_FORMAT_USER_ID)-1, &UserID))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): HexStrTo64Bit\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }

        pNewUserInfo->UserID = UserID;
        LOG_DEBUG("[S* load user] UserID: 0x%lx\n", pNewUserInfo->UserID);
        //Get UserID <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        //Get UserName >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
        if(NULL == fgets(buf, sizeof(buf), fp) || (0 != feof(fp)))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }
        
        LineCount++;

        if(0 != strncmp(SERVER_FORMAT_USER_NAME, buf, sizeof(SERVER_FORMAT_USER_NAME)-1))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }
        
        pTmp = buf + sizeof(SERVER_FORMAT_USER_NAME) - 1;
        pTmp2 = pNewUserInfo->UserName;
        for(i = 0; i < USER_NAME_MAX_LENGTH; i++)
        {
            if(('\n' == *pTmp) || ('\0' == *pTmp))
            {
                *pTmp2 = '\0';
                break;
            }

            *pTmp2++ = *pTmp++;
        }

        if((0 == i) || ((USER_NAME_MAX_LENGTH == i) && ('\0' != *pTmp)))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }

        pNewUserInfo->UserName[USER_NAME_MAX_LENGTH-1] = '\0';
        LOG_DEBUG("[S* load user] UserName: %s\n", pNewUserInfo->UserName);
        //Get UserName <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        //Get password >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
        if(NULL == fgets(buf, sizeof(buf), fp) || (0 != feof(fp)))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }
        
        LineCount++;

        if(0 != strncmp(SERVER_FORMAT_PASSWORD, buf, sizeof(SERVER_FORMAT_PASSWORD)-1))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }
        
        pTmp = buf + sizeof(SERVER_FORMAT_PASSWORD) - 1;
        pTmp2 = pNewUserInfo->password;
        for(i = 0; i < PASSWORD_MAX_LENGTH; i++)
        {
            if(('\n' == *pTmp) || ('\0' == *pTmp))
            {
                *pTmp2 = '\0';
                break;
            }

            *pTmp2++ = *pTmp++;
        }
        
        if((0 == i) || ((PASSWORD_MAX_LENGTH == i) && ('\0' != *pTmp)))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }
        
        pNewUserInfo->password[PASSWORD_MAX_LENGTH-1] = '\0';
        LOG_DEBUG("[S* load user] Password: %s\n", pNewUserInfo->password);
        //Get password <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
        
        //Get level >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
        if(NULL == fgets(buf, sizeof(buf), fp) || (0 != feof(fp)))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }

        LineCount++;

        if(0 != strncmp(SERVER_FORMAT_LEVEL, buf, sizeof(SERVER_FORMAT_LEVEL)-1))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }
        
        pTmp = buf + sizeof(SERVER_FORMAT_PASSWORD) - 1;
        if(('0' > *pTmp) || ('9' < *pTmp) || ('\n' != pTmp[1]))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }
        
        pNewUserInfo->level = *pTmp;
        LOG_DEBUG("[S* load user] Level: %d\n", pNewUserInfo->level);
        //Get level <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

        if(NULL == fgets(buf, sizeof(buf), fp) || (0 != feof(fp)))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }
        
        LineCount++;

        if(0 != strcmp(SERVER_FORMAT_END, buf))
        {
            free(pNewUserInfo);
            fclose(fp);
            LOG_ERROR("[S* load user] %s(line:%d): invalid format\n", SERVER_USER_LIST_FILE, LineCount);
            return STAT_ERR;
        }
        
        pNewUserInfo->pNext = NULL;
        pCurUserInfo->pNext = pNewUserInfo;
        pCurUserInfo = pNewUserInfo;
    }

    fclose(fp);

    return STAT_OK;
}

/*
 *  @Briefs: Save all info in g_HeadUserInfo to user.list
 *  @Return: STAT_OK / STAT_FATAL_ERR
 *  @Note:   Format: 
 *           <START/>
 *               <UserID/>   ...
 *               <UserName/> ...
 *               <Password/> ...
 *               <Level/>    ...
 *           <END/>
 */
static G_STATUS SERVER_SaveUserTable(void)
{
    FILE *fp;
    char FileName[FILE_NAME_MAX_LENGTH];
    UserInfo_t *pCurUserInfo;

    snprintf(FileName, sizeof(FileName), "%s_new", SERVER_USER_LIST_FILE);

    if(STAT_OK != CreateFile(FileName, 0600))
    {
        LOG_ERROR("[S* Save user] Fail to reate (%s): %s\n", FileName, strerror(errno));
        return STAT_ERR;
    }

    fp = fopen(FileName, "wb+");
    if(NULL == fp)
    {
        LOG_ERROR("[S* Save user] fopen(%s): %s\n", FileName, strerror(errno));
        return STAT_ERR;
    }

    pCurUserInfo = g_HeadUserInfo.pNext;

    while(NULL != pCurUserInfo)
    {
        pCurUserInfo->UserName[USER_NAME_MAX_LENGTH-1] = '\0';
        pCurUserInfo->password[PASSWORD_MAX_LENGTH-1] = '\0';
        
        fprintf(fp, "%s%s0x%lx\n%s%s\n%s%s\n%s%c\n%s", 
            SERVER_FORMAT_START, 
            SERVER_FORMAT_USER_ID, pCurUserInfo->UserID, 
            SERVER_FORMAT_USER_NAME, pCurUserInfo->UserName, 
            SERVER_FORMAT_PASSWORD, pCurUserInfo->password, 
            SERVER_FORMAT_LEVEL, pCurUserInfo->level,
            SERVER_FORMAT_END);

        pCurUserInfo = pCurUserInfo->pNext;
    }

    fclose(fp);

    if(0 != unlink(SERVER_USER_LIST_FILE))
    {
        LOG_WARNING("[S* Save user] unlink(%s): %s\n", SERVER_USER_LIST_FILE, strerror(errno));
    }

    if(0 != rename(FileName, SERVER_USER_LIST_FILE))
    {
        LOG_ERROR("[S* Save user] rename(): %s\n", strerror(errno));
        return STAT_ERR;
    }

    return STAT_OK;
}

/*
 *  @Briefs: Add user to g_HeadUserInfo and save in user.list
 *  @Return: STAT_OK / STAT_FATAL_ERR
 *  @Note:   None
 */
G_STATUS SERVER_AddUserPlus(MsgPkt_t *pMsgPkt)
{
    MsgDataAddUser_t *pMsgDataAddUser;
    pMsgDataAddUser = (MsgDataAddUser_t *)pMsgPkt->data;

    return STAT_OK;
}

/*
 *  @Briefs: Add user to g_HeadUserInfo and save in user.list
 *  @Return: STAT_OK / STAT_FATAL_ERR
 *  @Note:   None
 */
static G_STATUS SERVER_AddUser(const char *pUserName, const char *pPassword, uint8_t level)
{
    UserInfo_t *pCurUserInfo;
    UserInfo_t *pNewUserInfo;

    pNewUserInfo = (UserInfo_t *)malloc(sizeof(UserInfo_t));
    if(NULL == pNewUserInfo)
    {
        LOG_ERROR("[S* Add user] malloc(): %s\n", strerror(errno));
        return STAT_ERR;
    }

    pCurUserInfo = &g_HeadUserInfo;
    while(NULL != pCurUserInfo->pNext)
    {
        if(0 == strcmp(pUserName, pCurUserInfo->UserName))
        {
            free(pNewUserInfo);
            LOG_WARNING("[S* Add user] User has been exist: %s\n", pUserName);
            return STAT_ERR;
        }
        
        pCurUserInfo = pCurUserInfo->pNext;
    }

    pNewUserInfo->UserID = SERVER_CreateUserID();
    pNewUserInfo->level = level;
    strncpy(pNewUserInfo->UserName, pUserName, USER_NAME_MAX_LENGTH-1);
    strncpy(pNewUserInfo->password, pPassword, PASSWORD_MAX_LENGTH-1);
    pNewUserInfo->UserName[USER_NAME_MAX_LENGTH-1] = '\0';
    pNewUserInfo->password[PASSWORD_MAX_LENGTH-1] = '\0';
    
    pNewUserInfo->pNext = NULL;
    pCurUserInfo->pNext = pNewUserInfo;

    if(STAT_OK != SERVER_SaveUserTable())
        return STAT_ERR;

    return STAT_OK;
}

/*
 *  @Briefs: Delete user from g_HeadUserInfo and save in user.list
 *  @Return: STAT_OK / STAT_FATAL_ERR
 *  @Note:   None
 */
static G_STATUS SERVER_DelUser(uint64_t UserID)
{
    UserInfo_t *pCurUserInfo;
    UserInfo_t *pPrevUserInfo;

    pCurUserInfo = g_HeadUserInfo.pNext;
    pPrevUserInfo = &g_HeadUserInfo;
    
    while(NULL != pCurUserInfo)
    {
        if(UserID == pCurUserInfo->UserID)
            break;
        
        pPrevUserInfo = pCurUserInfo;
        pCurUserInfo = pCurUserInfo->pNext;
    }

    if(NULL == pCurUserInfo)
    {
        LOG_WARNING("[S* Del user] User is not found: 0x%lx\n", UserID);
        return STAT_ERR;
    }

    pPrevUserInfo->pNext = pCurUserInfo->pNext;
    free(pCurUserInfo);

    if(STAT_OK != SERVER_SaveUserTable())
        return STAT_ERR;

    return STAT_OK;
}

static G_STATUS SERVER_VerifyUser(UserInfo_t *pUserInfo)
{
    UserInfo_t *pCurUserInfo;

    pCurUserInfo = &g_HeadUserInfo;

    while(NULL != pCurUserInfo)
    {
        if(pUserInfo->UserID == pCurUserInfo->UserID)
            break;

        pCurUserInfo = pCurUserInfo->pNext;
    }

    if(NULL == pCurUserInfo)
    {
        LOG_WARNING("[S* Verify user] Not found user: %s\n", pUserInfo->UserName);
        return STAT_ERR;
    }

    if(0 != strcmp(pUserInfo->UserName, pCurUserInfo->UserName))
    {
        LOG_WARNING("[S* Verify user] UserID does not match UserName: 0x%lx - %s\n", 
            pUserInfo->UserID, pUserInfo->UserName);
        return STAT_ERR;
    }

    if(0 != strcmp(pUserInfo->password, pCurUserInfo->password))
    {
        LOG_WARNING("[S* Verify user] %s: password error\n", pUserInfo->UserName);
        return STAT_ERR;
    }

    return STAT_OK;
}

/*
 *  @Briefs: Do some initializing operation
 *  @Return: STAT_OK / STAT_ERR
 *  @Note:   None
 */
static G_STATUS SERVER_BeforeCreateTask(void)
{
    if(STAT_OK != SERVER_InitServerFile())
        return STAT_ERR;

    if(STAT_OK != SERVER_InitGlobalVaribles())
        return STAT_ERR;

    if(0 == access(SERVER_TRANSFER_QUEUE, F_OK))
    {
        if(0 != unlink(SERVER_TRANSFER_QUEUE))
        {
            LOG_FATAL_ERROR("[S* Create queue] unlink(): %s", strerror(errno));
            return STAT_FATAL_ERR;
        }
    }
    
    if(0 != mkfifo(SERVER_TRANSFER_QUEUE, 0600))
    {
        LOG_FATAL_ERROR("[S* Create queue] mkfifo(): %s", strerror(errno));
        return STAT_FATAL_ERR;
    }
    
    g_ServerTransferFd = open(SERVER_TRANSFER_QUEUE, O_RDWR);
    if(0 > g_ServerTransferFd)
    {
        LOG_FATAL_ERROR("[S* Create queue] open(): %s", strerror(errno));
        return STAT_FATAL_ERR;
    }

    if(STAT_OK != SERVER_LoadUserTable())
        return STAT_ERR;

    return STAT_OK;
}

/*
 *  @Briefs: Configure server network function
 *  @Return: Return socket fd value if success, otherwise return -1
 *  @Note: None
 */
static int SERVER_ConfigServer(struct sockaddr_in *pServerSocketAddr)
{
    int SocketFd;
    int res;
    int RecvBufLength;
    int SendBufLength;
    
    SocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if(0 > SocketFd)
    {
        LOG_FATAL_ERROR("[S* config] socket(): %s\n", strerror(errno));
        return -1;
    }

    RecvBufLength = SERVER_SOCKET_RECV_BUF_SIZE;
    if(0 != setsockopt(SocketFd, SOL_SOCKET, SO_RCVBUF, &RecvBufLength, sizeof(int)))
    {
        LOG_FATAL_ERROR("[S* config] setsockopt(): %s\n", strerror(errno));
        return -1;
    }

    SendBufLength = SERVER_SOCKET_SEND_BUF_SIZE;
    if(0 != setsockopt(SocketFd, SOL_SOCKET, SO_SNDBUF, &SendBufLength, sizeof(int)))
    {
        LOG_FATAL_ERROR("[S* config] setsockopt(): %s\n", strerror(errno));
        return -1;
    }
    
    memset(pServerSocketAddr, 0, sizeof(struct sockaddr_in));
    pServerSocketAddr->sin_family = AF_INET;
    pServerSocketAddr->sin_addr.s_addr = htonl(INADDR_ANY);
    pServerSocketAddr->sin_port = htons(8080);
    
    res = bind(SocketFd, (struct sockaddr*)pServerSocketAddr, sizeof(struct sockaddr_in));
    if(0 != res)
    {
        LOG_FATAL_ERROR("[S* config] bind(): %s\n", strerror(errno));
        return -1;
    }
    
    res = listen(SocketFd, 10);
    if(0 != res)
    {
        close(SocketFd);
        LOG_FATAL_ERROR("[S* config] listen(): %s\n", strerror(errno));
        return -1;
    }

    return SocketFd;
}

/*
 *  @Briefs: Find out the session
 *  @Return: None
 *  @Note:   1. Require to lock session before invoking
 *           2. Set value only if ppCurSession or ppPrevSession is not NULL
 *           3. If fd is negative, it will find the session based on UserID
 */
static void SERVER_GetSession(session_t **ppPrevSession, session_t **ppCurSession, 
    int fd, uint64_t UserID)
{
    session_t *pCurSession;
    session_t *pPrevSession;

    pCurSession = &g_HeadSession;

    if(0 < fd)
    {
        while(1)
        {
            pPrevSession = pCurSession;
            pCurSession = pCurSession->pNext;
            if(NULL == pCurSession)
                break;

            if(fd == pCurSession->fd)
                break;
        }
    }
    else
    {
        while(1)
        {
            pPrevSession = pCurSession;
            pCurSession = pCurSession->pNext;
            if(NULL == pCurSession)
                break;

            if(UserID == pCurSession->pUserInfo->UserID)
                break;
        }
    }

    if(NULL != ppPrevSession)
    {
        *ppPrevSession = pPrevSession;
    }

    if(NULL != ppCurSession)
    {
        *ppCurSession = pCurSession;
    }
}

/*
 *  @Briefs: Calculate the max fd value according to all sessions
 *  @Return: None
 *  @Note:   Require to lock session before invoking
 */
static void SERVER_UpdateMaxFd(__IO int *pMaxFd)
{
    int MaxFd;
    session_t *pCurSession;

    MaxFd = g_ServerSocketFd;
    pCurSession = g_HeadSession.pNext;
    
    while(NULL != pCurSession)
    {
        if(MaxFd < pCurSession->fd)
        {
            MaxFd = pCurSession->fd;
        }
                                        
        pCurSession = pCurSession->pNext;
    }

    *pMaxFd = MaxFd;
    LOG_DEBUG("[Update MaxFd] MaxFd=%d\n", MaxFd);
}

/*
 *  @Briefs: Free the memory of session and connect the previous session with the next session
 *  @Return: None
 *  @Note:   1. Require to lock session before invoking
 *           2. If flag is TRUE, it means it needs to update MaxFd
 */
static void SERVER_FreeSession(session_t *pPrevSession, session_t *pCurSession, _BOOL_ flag)
{
    int fd;

    if((NULL == pPrevSession) || (NULL == pCurSession))
        return;

    if(g_pTailSession == pCurSession)
    {
        g_pTailSession = pPrevSession;
    }
    
    pPrevSession->pNext = pCurSession->pNext;
    fd = pCurSession->fd;
    if(0 <= fd)
    {
        close(fd);
    }

    free(pCurSession);
    g_HeadSession.fd--;
    
    FD_CLR(fd, &g_PrevFds);
    if((TRUE == flag) && (fd == g_MaxFd))
    {
        SERVER_UpdateMaxFd(&g_MaxFd);
    }
    
    LOG_DEBUG("[S* free session][0x%lx] success\n", (int64_t)pCurSession);
}

/*
 *  @Briefs: Create session
 *  @Return: STAT_OK / STAT_ERR
 *  @Note: None
 */
static G_STATUS SERVER_CreateSession(int fd, struct sockaddr_in *pClientSocketAddr)
{
    if(0 > fd)
        return STAT_ERR;
    
    session_t *pNewSession;

    pNewSession = (session_t *)calloc(1, sizeof(session_t));
    if(NULL == pNewSession)
    {
        LOG_ERROR("[Create session] calloc(): %s\n", strerror(errno));
        return STAT_ERR;
    }

    pNewSession->fd = fd;
    pNewSession->status = SESSION_STATUS_CREATE;
    if(NULL != pClientSocketAddr)
    {
        memcpy(pNewSession->ip, inet_ntoa(pClientSocketAddr->sin_addr), IP_ADDR_MAX_LENGTH);
    }
    pNewSession->HoldTime = 0;
    pNewSession->pUserInfo = NULL;
    pNewSession->pNext = NULL;

    pthread_mutex_lock(&g_SessionLock);
    g_pTailSession->pNext = pNewSession;
    g_pTailSession = pNewSession;
    g_HeadSession.fd++;
    
    FD_SET(fd, &g_PrevFds);
    if(g_MaxFd < fd)
    {
        g_MaxFd = fd;
    }
    
    LOG_DEBUG("[Create session] New session %s, fd=%d\n", pNewSession->ip, fd);
    
    pthread_mutex_unlock(&g_SessionLock);
    
    return STAT_OK;
}

/*
 *  @Briefs: Close the session and free memory
 *  @Return: STAT_OK / STAT_ERR
 *  @Note: If fd is negative, it will close the session based on UserID
 */
static G_STATUS SERVER_CloseSession(int fd, uint64_t UserID)
{
    session_t *pPrevSession;
    session_t *pCurSession;

    pPrevSession = NULL;
    pCurSession = NULL;
    pthread_mutex_lock(&g_SessionLock);

    SERVER_GetSession(&pPrevSession, &pCurSession, fd, UserID);
    if((NULL == pPrevSession) || (NULL == pCurSession))
    {
        pthread_mutex_unlock(&g_SessionLock);
        return STAT_ERR;
    }

    SERVER_FreeSession(pPrevSession, pCurSession, TRUE);
    
    pthread_mutex_unlock(&g_SessionLock);

    return STAT_OK;
}

/*
 *  @Briefs: Send message to specified fd
 *  @Return: STAT_OK / STAT_ERR
 *  @Note:   pMsgPkt->fd and pMsgPkt->DataSize must be set value before invoked
 */
static G_STATUS SERVER_SendResponse(MsgPkt_t *pMsgPkt)
{
    int WriteDataLength;

    if(0 > pMsgPkt->fd)
        return STAT_ERR;

    pMsgPkt->NetFn = MSG_NET_FN_COMMON;
    pMsgPkt->cmd = MSG_CMD_SEND_RES;
    pMsgPkt->flag = 0;
    pMsgPkt->DataCRC = CRC32_calculate(&pMsgPkt->data, pMsgPkt->DataSize);
    pMsgPkt->PayloadCRC = CRC16_calculate(pMsgPkt, MSG_PALYPOAD_CRC_SIZE);

    WriteDataLength = write(pMsgPkt->fd, pMsgPkt, MSG_PALYPOAD_SIZE+pMsgPkt->DataSize);
    if((MSG_PALYPOAD_SIZE+pMsgPkt->DataSize) != WriteDataLength)
        return STAT_ERR;
    
    return STAT_OK;
}
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//Static function





#define COMMON_FUNC //Only use for locating function efficiently
//Common function
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
G_STATUS SERVER_CreateTask(pthread_t *pServerTaskID)
{
    int res;
    pthread_t ServerTaskID;
    pthread_attr_t ServerTaskAttr;
    
    if(STAT_OK != SERVER_BeforeCreateTask())
        return STAT_ERR;
    
    res = pthread_attr_init(&ServerTaskAttr);
    if(0 != res)
    {
        LOG_FATAL_ERROR("[S* create task] Fail to init thread attribute\n");
        return STAT_ERR;
    }
    
    res = pthread_attr_setdetachstate(&ServerTaskAttr, PTHREAD_CREATE_DETACHED);
    if(0 != res)
    {
        LOG_FATAL_ERROR("[S* create task] Fail to set thread attribute\n");
        return STAT_ERR;
    }
    
    res = pthread_create(&ServerTaskID, &ServerTaskAttr, SERVER_ServerTask, NULL);
    if(0 != res)
    {
        LOG_FATAL_ERROR("[S* create task] Fail to create task\n");
        return STAT_ERR;
    }
    
    pthread_attr_destroy(&ServerTaskAttr);
    *pServerTaskID = ServerTaskID;

    return STAT_OK;
}

void *SERVER_ServerTask(void *pArg)
{
    int ServerSocketFd;
    struct sockaddr_in ServerSocketAddr;
    struct timeval TimeInterval;
    fd_set fds;
    int res;
    socklen_t ClientSocketAddrLen;
    int ClientSocketFd;
    struct sockaddr_in ClientSocketAddr;
    int ReadDataLength;
    MsgPkt_t MsgPkt;
    MsgDataRes_t *pResMsgPkt;
    session_t *pPrevSession;
    session_t *pCurSession;
    int CurFd;

    ServerSocketFd = SERVER_ConfigServer(&ServerSocketAddr);
    if(0 > ServerSocketFd)
        return NULL;

    g_ServerSocketFd = ServerSocketFd;
    
    MSG_InitMsgPkt(&MsgPkt);
    pResMsgPkt = (MsgDataRes_t *)&MsgPkt.data;
    TimeInterval.tv_usec = 0;
    g_MaxFd = ServerSocketFd;
    FD_ZERO(&g_PrevFds);
    FD_SET(ServerSocketFd, &g_PrevFds);
 
    while(1)
    {
        g_ServerLiveFlag = 1;
        fds = g_PrevFds;
        TimeInterval.tv_sec = SERVER_SELECT_TIME_INTERVAL;

        res = select(g_MaxFd+1, &fds, NULL, NULL, &TimeInterval);
        if(0 > res)
        {
            LOG_DEBUG("[S* task] select(): force to close fd\n");
            continue;
        }

        if(res == 0) //Timeout
            continue;
        
        if(FD_ISSET(ServerSocketFd, &fds)) //New connection
        {
            ClientSocketAddrLen = sizeof(struct sockaddr_in);
            ClientSocketFd = accept(ServerSocketFd, (struct sockaddr*)&ClientSocketAddr, 
                &ClientSocketAddrLen);
            if(0 > ClientSocketFd)
            {
                LOG_WARNING("[S* task] accept(): %s\n", strerror(errno));
                continue;
            }

            SERVER_CreateSession(ClientSocketFd, &ClientSocketAddr);

            MsgPkt.fd = ClientSocketFd;
            pResMsgPkt->CC = CC_NORMAL;
            MsgPkt.DataSize = sizeof(MsgDataRes_t);
            SERVER_SendResponse(&MsgPkt);
            
            continue;
        }

        pthread_mutex_lock(&g_SessionLock); //Pay attention to unlock
        pPrevSession = &g_HeadSession;
        pCurSession = g_HeadSession.pNext;
        while(1)
        {
            if(NULL == pCurSession)
            {
                pthread_mutex_unlock(&g_SessionLock);
                break;
            }
            
            if(!FD_ISSET(pCurSession->fd, &fds))
            {
                pPrevSession = pCurSession;
                pCurSession = pCurSession->pNext;
                continue;
            }
            
            ReadDataLength = read(pCurSession->fd, (char *)&MsgPkt, sizeof(MsgPkt_t));
            if(0 > ReadDataLength)
            {
                pthread_mutex_unlock(&g_SessionLock);
                LOG_DEBUG("[S* task][%s] Fail to read: may be abnormal disconnection\n", pCurSession->ip);
                SERVER_FreeSession(pPrevSession, pCurSession, TRUE);
                pCurSession = pPrevSession->pNext;
                break;
            }
            
            if(0 == ReadDataLength)
            {
                LOG_DEBUG("[S* task][%s] Abnormal disconnection\n", pCurSession->ip);
                SERVER_FreeSession(pPrevSession, pCurSession, TRUE);
                pCurSession = pPrevSession->pNext;
                continue;
            }
            
            CurFd = pCurSession->fd;
            LOG_DEBUG("[S* task][%s] New message\n", pCurSession->ip);
            pthread_mutex_unlock(&g_SessionLock);

            if((sizeof(MsgPkt_t) == ReadDataLength)  && (MSG_CMD_DO_NOTHING != MsgPkt.cmd))
            {
                //Make sure close session operation could only close the session of msg source own
                if(CurFd == MsgPkt.fd)
                {
                    MSG_PostMsg(&MsgPkt);
                }
                else
                {
                    LOG_DEBUG("[S* task] Msg fd does not match session fd\n");
                }
            }
            
            break;
        }
    }
    
    return NULL;
}
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//Common function





#define ROOT_LEVEL //Only use for locating function efficiently
//Root level function
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//Root level function





#define ADMIN_LEVEL //Only use for locating function efficiently
//Admin level function
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
//Admin level function
