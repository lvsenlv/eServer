/*************************************************************************
	> File Name: eTools.c
	> Author: 
	> Mail: 
	> Created Time: 2018年04月19日 星期四 09时55分20秒
 ************************************************************************/

#include "hex_str_to_dec.h"
#include "server.h"
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <time.h>

static void DispHelpInfo(const char *pProjectName)
{
    DISP("Version 1.8.13\n");
    
    DISP("\nUsage: %s [options...] <commands>\n", pProjectName);
    
    DISP("\n[Options]:\n");
    DISP("    -h        This help\n");
    DISP("    -H        Hostname - remote host name for LAN interface\n");
    DISP("    -i/-ip    Same as -H\n");
    DISP("    -I/-IP    Same as -H\n");
    DISP("    -u/-U     Username - remote session username\n");
    DISP("    -p/-P     Password - remote session password\n");
    
    DISP("\n<Commands>:\n");
    DISP("    power off      Make remote system power off\n");
    DISP("    reboot         Make remote system reboot\n");
    
}

static G_STATUS CheckParameter(int argc, char **argv, int index, int length)
{
    if((index+1) >= argc)
    {
        DISP_ERR("-%c: require parameter\n", argv[index][1]);
        return STAT_ERR;
    }
    else if('-' == argv[index+1][0])
    {
        DISP_ERR("-%c: invalid parameter %s\n", argv[index][1], argv[index+1]);
        return STAT_ERR;
    }

    if((1 == length) && ('\0' != argv[index][1]) && ('\0' != argv[index][2]))
    {
        DISP_ERR("%s: invalid option\n", argv[index]);
        return STAT_ERR;
    }

    return STAT_OK;
}

static G_STATUS ParseCommandLine(int argc, char **argv, ArgInfo_t *pArgInfo)
{
    int i;
    int CurCmdPos;
    int DataLength;

    if(2 > argc)
    {
        DispHelpInfo(argv[0]);
        return STAT_ERR;
    }

    CurCmdPos = 0;
    memset(pArgInfo, 0, sizeof(ArgInfo_t));

    for(i = 1; i < argc; i++)
    {
        if('-' != argv[i][0])
        {
            //If snprintf execuetes successfully, the DataLength means the actual length of source string
            DataLength = snprintf(&pArgInfo->cmd[CurCmdPos], sizeof(pArgInfo->cmd)-CurCmdPos, "%s", argv[i]);
            CurCmdPos += DataLength;

            if(sizeof(pArgInfo->cmd) <= CurCmdPos)
            {
                DISP_ERR("Command is too long\n");
                return STAT_ERR;
            }
            
            pArgInfo->cmd[CurCmdPos++] = ' ';
            
            if(sizeof(pArgInfo->cmd) == CurCmdPos)
            {
                pArgInfo->cmd[CurCmdPos-1] = '\0';
                continue;
            }
            
            continue;
        }

        switch(argv[i][1])
        {
            case 'h':
                DispHelpInfo(argv[0]);
                break;
            case 'i':
                if(STAT_OK != CheckParameter(argc, argv, i, 0))
                    return STAT_ERR;

                if(('p' == argv[i][2]) && ('\0' != argv[i][3]))
                {
                    DISP_ERR("%s: invalid option\n", argv[i]);
                    return STAT_ERR;
                }
                else if(('p' != argv[i][2]) && ('\0' != argv[i][2]))
                {
                    DISP_ERR("%s: invalid option\n", argv[i]);
                    return STAT_ERR;
                }
                
                i++;
                snprintf(pArgInfo->IPAddr, sizeof(pArgInfo->IPAddr), "%s", argv[i]);
                
                break;
            case 'I':
                if(STAT_OK != CheckParameter(argc, argv, i, 0))
                    return STAT_ERR;

                if(('P' == argv[i][2]) && ('\0' != argv[i][3]))
                {
                    DISP_ERR("%s: invalid option\n", argv[i]);
                    return STAT_ERR;
                }
                else if(('P' != argv[i][2]) && ('\0' != argv[i][2]))
                {
                    DISP_ERR("%s: invalid option\n", argv[i]);
                    return STAT_ERR;
                }
                
                i++;
                snprintf(pArgInfo->IPAddr, sizeof(pArgInfo->IPAddr), "%s", argv[i]);
                
                break;
            case 'H':
                if(STAT_OK != CheckParameter(argc, argv, i, 1))
                    return STAT_ERR;

                if(('i' == argv[i][1]) && ('p' == argv[i][2]) && ('\0' != argv[i][3]))
                {
                    DISP_ERR("%s: invalid option\n", argv[i]);
                    return STAT_ERR;
                }
                
                i++;
                snprintf(pArgInfo->IPAddr, sizeof(pArgInfo->IPAddr), "%s", argv[i]);
                
                break;
            case 'u':
            case 'U':
                if(STAT_OK != CheckParameter(argc, argv, i, 1))
                    return STAT_ERR;
                
                i++;
                snprintf(pArgInfo->UserName, sizeof(pArgInfo->UserName), "%s", argv[i]);
                
                break;
            case 'p':
            case 'P':
                if(STAT_OK != CheckParameter(argc, argv, i, 1))
                    return STAT_ERR;
                
                i++;
                snprintf(pArgInfo->password, sizeof(pArgInfo->password), "%s", argv[i]);
                
                break;
            default:
                DISP_ERR("Unknown option: -%c\n", argv[i][1]);
                return STAT_ERR;
                
                break;
        }
    }

    if(' ' == pArgInfo->cmd[CurCmdPos-1])
    {
        pArgInfo->cmd[CurCmdPos-1] = '\0';
    }

    if('\0' == pArgInfo->IPAddr[0])
    {
        DISP_ERR("Error: hostname is required\n");
        return STAT_ERR;
    }
    
    if('\0' == pArgInfo->UserName[0])
    {
        DISP_ERR("Error: UserName is required\n");
        return STAT_ERR;
    }
    
    if('\0' == pArgInfo->password[0])
    {
        DISP_ERR("Error: password is required\n");
        return STAT_ERR;
    }
    
    if('\0' == pArgInfo->cmd[0])
    {
        DISP_ERR("Error: command is required\n");
        return STAT_ERR;
    }
    
    return STAT_OK;
}

static G_STATUS SERVER_ShowUserList(void);
static G_STATUS SERVER_LoadUserTable(void);
static G_STATUS SERVER_SaveUserTable(void);
static G_STATUS SERVER_AddUser(const char *pUserName, const char *pPassword, uint8_t level);
static G_STATUS SERVER_DelUser(uint64_t UserID);

UserInfo_t g_HeadUserInfo;

int main(int argc, char **argv)
{
#if 0
    ArgInfo_t ArgInfo;
    
    if(STAT_OK != ParseCommandLine(argc, argv, &ArgInfo))
        return -1;

    DISP("IPAddr: %s\n", ArgInfo.IPAddr);
    DISP("UserName: %s\n", ArgInfo.UserName);
    DISP("password: %s\n", ArgInfo.password);
    DISP("Cmd: %s\n", ArgInfo.cmd);
#endif

    if(STAT_OK != SERVER_LoadUserTable())
        return -1;

    SERVER_AddUser("NewUser", "NewPassword", '9');
    
    return 0;
}

#define LOG_INFO printf
#define LOG_ERROR printf
#define LOG_FATAL_ERROR printf
#define LOG_DEBUG printf
#define LOG_WARNING printf

static uint64_t SERVER_CreateUserID(void)
{
    uint64_t UserID;
    struct timeval TimeInterval;
    
    gettimeofday(&TimeInterval, NULL);
    UserID = TimeInterval.tv_sec;

    return UserID;
}

static G_STATUS SERVER_ShowUserList(void)
{
    UserInfo_t *pCurUserInfo;

    pCurUserInfo = &g_HeadUserInfo;

    while(NULL != pCurUserInfo)
    {
        LOG_DEBUG("UserID: 0x%lx\n", pCurUserInfo->UserID);
        LOG_DEBUG("UserName: %s\n", pCurUserInfo->UserName);
        LOG_DEBUG("Password: %s\n", pCurUserInfo->password);

        pCurUserInfo = pCurUserInfo->pNext;
    }

    return STAT_OK;
}
