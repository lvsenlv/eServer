/*************************************************************************
	> File Name: main.c
	> Author: 
	> Mail: 
	> Created Time: 2018年01月29日 星期一 08时54分56秒
 ************************************************************************/

#include "message.h"
#include "completion_code.h"
#include "crc.h"
#include <errno.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>

#define __ROOT_LOGIN
#ifdef __ROOT_LOGIN
#define LOGIN_USER_NAME                     "lvsenlv"
#define LOGIN_PASSWORD                      "linuxroot"
#define LOGIN_USER_ID                       4884
#else
#define LOGIN_USER_NAME                     "admin01"
#define LOGIN_PASSWORD                      "admin123"
#endif

G_STATUS FillMsgPkt(MsgPkt_t *pMsgPkt, char ch, _BOOL_ *pFlag);
G_STATUS GetResponse(MsgPkt_t *pMsgPkt, int fd, int timeout);
void DispHelpInfo(void);
G_STATUS GetFileFromServer(MsgPkt_t *pMsgPkt, int fd, int timeout);
 
int g_ResFd;
int g_ClientSocketFd;
 
int main(int argc, char **argv)
{
    if(2 != argc)
    {
        printf("please input ip address\n");
        return -1;
    }

    if(0 == strcmp("-h", argv[1]))
    {
        DispHelpInfo();
        return 0;
    }

    InitErrorCodeTable();
    CRC32_InitTable();
    CRC16_InitTable();

    int ClientSocketFd = 0;
    struct sockaddr_in ClientSocketAddr;
    int res;
    fd_set ReadFds;
    int MaxFd = 0;
    char buf[BUF_SIZE] = {0};
    int SendDataLength;
    MsgPkt_t MsgPkt;
    MsgPkt_t ResMsgPkt;
    MsgDataVerifyIdentity_t *pVerifyData;
    MsgDataRes_t *pResMsgData;
    int retry;
    struct timeval tv;
    _BOOL_ flag;
    int RecvBufLength;
    int SendBufLength;
    int OptLength = sizeof(int);
    
    ClientSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if(0 > ClientSocketFd)
    {
        perror("create socket failed");
        return -1;
    }
    
    if(ClientSocketFd > MaxFd)
    {
        MaxFd = ClientSocketFd;
    }

    g_ClientSocketFd = ClientSocketFd;
    
    RecvBufLength = 64*1024;
    if(0 != setsockopt(ClientSocketFd, SOL_SOCKET, SO_RCVBUF, &RecvBufLength, sizeof(int)))
    {
        printf("setsockopt(): %s\n", strerror(errno));
        return -1;
    }
    
    SendBufLength = 64*1024;
    if(0 != setsockopt(ClientSocketFd, SOL_SOCKET, SO_SNDBUF, &SendBufLength, sizeof(int)))
    {
        printf("setsockopt(): %s\n", strerror(errno));
        return -1;
    }
    
    if(0 != getsockopt(ClientSocketFd, SOL_SOCKET, SO_RCVBUF, &RecvBufLength, (socklen_t *)&OptLength))
    {
        printf("setsockopt(): %s\n", strerror(errno));
        return -1;
    }

    printf("RecvBufLength=%d\n", RecvBufLength);

    if(0 != getsockopt(ClientSocketFd, SOL_SOCKET, SO_SNDBUF, &SendBufLength, (socklen_t *)&OptLength))
    {
        printf("setsockopt(): %s\n", strerror(errno));
        return -1;
    }
    
    printf("SendBufLength=%d\n", SendBufLength);
    
    memset(&ClientSocketAddr, 0, sizeof(struct sockaddr_in));
    ClientSocketAddr.sin_family = AF_INET;
    ClientSocketAddr.sin_port = htons(8080);
    
    res = inet_pton(AF_INET, argv[1], &ClientSocketAddr.sin_addr);
    if(0 > res)
    {
        return -1;
    }

    memset(&MsgPkt, 0, sizeof(MsgPkt_t));
    memset(&ResMsgPkt, 0, sizeof(MsgPkt_t));
    pVerifyData = (MsgDataVerifyIdentity_t *)&MsgPkt.data;
    pResMsgData = (MsgDataRes_t *)&ResMsgPkt.data;

#ifdef __ROOT_LOGIN
    MsgPkt.NetFn = MSG_NET_FN_ROOT;
    MsgPkt.cmd = MSG_CMD_ROOT_LOGIN;
    pVerifyData->UserID = LOGIN_USER_ID;
#else
    MsgPkt.NetFn = MSG_NET_FN_ADMIN;
    MsgPkt.cmd = MSG_CMD_USER_LOGIN;
#endif
    MSG_SET_RES(MsgPkt.flag);
    memcpy(pVerifyData->UserName, LOGIN_USER_NAME, sizeof(LOGIN_USER_NAME)-1);
    memcpy(pVerifyData->password, LOGIN_PASSWORD, sizeof(LOGIN_PASSWORD)-1);
    MsgPkt.DataSize = sizeof(MsgDataVerifyIdentity_t);
    MsgPkt.DataCRC = CRC32_calculate(pVerifyData, sizeof(MsgDataVerifyIdentity_t));
    
    res = connect(ClientSocketFd, (struct sockaddr *)&ClientSocketAddr, sizeof(struct sockaddr_in));
    if(0 > res)
    {
        perror("connect");
        return -1;
    }
    
    for(retry = 1; retry <= 3; retry++)
    {
        if(STAT_OK != GetResponse(&ResMsgPkt, ClientSocketFd, 1))
            continue;
        
        g_ResFd = ResMsgPkt.fd;
        MsgPkt.fd = ResMsgPkt.fd;
        MsgPkt.PayloadCRC = CRC16_calculate(&MsgPkt, MSG_PALYPOAD_CRC_SIZE);
        printf("Server response fd: %d\n", g_ResFd);
        
        SendDataLength = write(ClientSocketFd, &MsgPkt, sizeof(MsgPkt_t));
        if(sizeof(MsgPkt_t) != SendDataLength)
        {
            printf("write(): %s\n", strerror(errno));
            return -1;
        }
        
        if(STAT_OK != GetResponse(&ResMsgPkt, ClientSocketFd, 1))
            continue;
        
        if(CC_NORMAL != pResMsgData->CC)
        {
            printf("%s\n", GetErrorDetails(pResMsgData->CC));
            return -1;
        }
            
        break;
    }

    if(3 < retry)
    {
        printf("Fail to connect server: server no response\n");
        return -1;
    }
    
    printf("Success connect to server\n");
    flag = FALSE;
    tv.tv_usec = 0;
    
    while(1)
    {
        while(1)
        {
            //fgets(buf, 2, stdin);
            FD_ZERO(&ReadFds);
            FD_SET(0, &ReadFds);
            tv.tv_sec = 9;
            
            res = select(1, &ReadFds, NULL, NULL,&tv);
            if(-1 == res)
            {
                perror("select:");
                return 0;
            }

            if(0 == res)
            {
                buf[0] = '0';
            }
            else
            {
                buf[0] = '\0';
                if(FD_ISSET(0, &ReadFds))
                {
                    read(STDIN_FILENO, buf, sizeof(buf));
                }
                
                if('\0' == buf[0])
                    continue;
            }
//            buf[0] = '7';
//            res = 1;

            if(STAT_OK != FillMsgPkt(&MsgPkt, buf[0], &flag))
                continue;

            if(MSG_NET_FN_DOWNLOAD == MsgPkt.NetFn)
                break;

            SendDataLength = write(ClientSocketFd, &MsgPkt, sizeof(MsgPkt_t));
            if(0 < SendDataLength)
            {
                printf("Success sending message\n");
            }
            else
            {
                printf("Fail to send message\n");
                continue;
            }

            break;
        }
        
        if(MSG_NET_FN_DOWNLOAD == MsgPkt.NetFn)
        {
            GetFileFromServer(NULL, ClientSocketFd, 3);
            continue;
        }

        if(0 == res)
            continue;

        if(TRUE != flag)
            continue;

        if(STAT_OK != GetResponse(&ResMsgPkt, ClientSocketFd, 1))
        {
            printf("Server no response\n");
            continue;
        }
        
        if(CC_NORMAL != pResMsgData->CC)
            printf("%s\n", GetErrorDetails(pResMsgData->CC));
        else
            printf("Success\n");
    }

    return 0;
}

G_STATUS FillMsgPkt(MsgPkt_t *pMsgPkt, char choice, _BOOL_ *pFlag)
{
    MsgDataAddUser_t *pMsgDataAddUser;
    MsgDataDelUser_t *pMsgDataDelUser;
    MsgDataRenameUser_t *pMsgDataRenameUser;

    memset(pMsgPkt, 0, sizeof(MsgPkt_t));
    MSG_SET_RES(pMsgPkt->flag);
    pMsgPkt->fd = g_ResFd;
    *pFlag = FALSE;
    
    pMsgPkt->NetFn = MSG_NET_FN_COMMON;
    pMsgPkt->cmd = MSG_CMD_DO_NOTHING;
    pMsgPkt->DataSize = 0;
    pMsgPkt->DataCRC = CRC32_calculate(&pMsgPkt->data, 0);
    pMsgPkt->PayloadCRC = CRC16_calculate(pMsgPkt, MSG_PALYPOAD_CRC_SIZE);
    
    switch(choice)
    {
        case '0':
            pMsgPkt->NetFn = MSG_NET_FN_COMMON;
            pMsgPkt->cmd = MSG_CMD_DO_NOTHING;
            pMsgPkt->DataSize = 0;
            pMsgPkt->DataCRC = CRC32_calculate(&pMsgPkt->data, 0);
            pMsgPkt->PayloadCRC = CRC16_calculate(pMsgPkt, MSG_PALYPOAD_CRC_SIZE);
            break;
        case '1':
#ifdef __ROOT_LOGIN
            *pFlag = TRUE;
            pMsgPkt->NetFn = MSG_NET_FN_ROOT;
            pMsgPkt->cmd = MSG_CMD_ROOT_ADD_ADMIN;
            pMsgDataAddUser = (MsgDataAddUser_t *)pMsgPkt->data;
            pMsgDataAddUser->VerifyData.UserID = LOGIN_USER_ID;
            memcpy(pMsgDataAddUser->VerifyData.UserName, LOGIN_USER_NAME, sizeof(LOGIN_USER_NAME)-1);
            memcpy(pMsgDataAddUser->VerifyData.password, LOGIN_PASSWORD, sizeof(LOGIN_PASSWORD)-1);
            memcpy(pMsgDataAddUser->AddUserName, "admin01", 7);
            memcpy(pMsgDataAddUser->AddPassword, "admin123", 8);
            pMsgPkt->DataSize = sizeof(MsgDataAddUser_t);
            pMsgPkt->DataCRC = CRC32_calculate(&pMsgPkt->data, sizeof(MsgDataAddUser_t));
            pMsgPkt->PayloadCRC = CRC16_calculate(pMsgPkt, MSG_PALYPOAD_CRC_SIZE);
#else
            printf("No root login\n");
#endif
            break;
        case '2':
#ifdef __ROOT_LOGIN
            *pFlag = TRUE;
            pMsgPkt->NetFn = MSG_NET_FN_ROOT;
            pMsgPkt->cmd = MSG_CMD_ROOT_DEL_ADMIN;
            pMsgDataDelUser = (MsgDataDelUser_t *)pMsgPkt->data;
            pMsgDataDelUser->VerifyData.UserID = LOGIN_USER_ID;
            memcpy(pMsgDataDelUser->VerifyData.UserName, LOGIN_USER_NAME, sizeof(LOGIN_USER_NAME)-1);
            memcpy(pMsgDataDelUser->VerifyData.password, LOGIN_PASSWORD, sizeof(LOGIN_PASSWORD)-1);
            memcpy(pMsgDataDelUser->DelUserName, "admin01", 7);
            pMsgPkt->DataSize = sizeof(MsgDataDelUser_t);
            pMsgPkt->DataCRC = CRC32_calculate(&pMsgPkt->data, sizeof(MsgDataDelUser_t));
            pMsgPkt->PayloadCRC = CRC16_calculate(pMsgPkt, MSG_PALYPOAD_CRC_SIZE);
#else
            printf("No root login\n");
#endif
            break;
        case '3':
            *pFlag = TRUE;
            pMsgPkt->NetFn = MSG_NET_FN_ADMIN;
            pMsgPkt->cmd = MSG_CMD_ADMIN_ADD_USER;
            pMsgDataAddUser = (MsgDataAddUser_t *)pMsgPkt->data;
            memcpy(pMsgDataAddUser->VerifyData.UserName, LOGIN_USER_NAME, sizeof(LOGIN_USER_NAME)-1);
            memcpy(pMsgDataAddUser->VerifyData.password, LOGIN_PASSWORD, sizeof(LOGIN_PASSWORD)-1);
            memcpy(pMsgDataAddUser->AddUserName, "user01", 6);
            memcpy(pMsgDataAddUser->AddPassword, "user123", 7);
            pMsgPkt->DataSize = sizeof(MsgDataAddUser_t);
            pMsgPkt->DataCRC = CRC32_calculate(&pMsgPkt->data, sizeof(MsgDataAddUser_t));
            pMsgPkt->PayloadCRC = CRC16_calculate(pMsgPkt, MSG_PALYPOAD_CRC_SIZE);
            break;
        case '4':
            *pFlag = TRUE;
            pMsgPkt->NetFn = MSG_NET_FN_ADMIN;
            pMsgPkt->cmd = MSG_CMD_ADMIN_DEL_USER;
            pMsgDataDelUser = (MsgDataDelUser_t *)pMsgPkt->data;
            memcpy(pMsgDataDelUser->VerifyData.UserName, LOGIN_USER_NAME, sizeof(LOGIN_USER_NAME)-1);
            memcpy(pMsgDataDelUser->VerifyData.password, LOGIN_PASSWORD, sizeof(LOGIN_PASSWORD)-1);
            memcpy(pMsgDataDelUser->DelUserName, "user01", 7);
            pMsgPkt->DataSize = sizeof(MsgDataDelUser_t);
            pMsgPkt->DataCRC = CRC32_calculate(&pMsgPkt->data, sizeof(MsgDataDelUser_t));
            pMsgPkt->PayloadCRC = CRC16_calculate(pMsgPkt, MSG_PALYPOAD_CRC_SIZE);
            break;
        case '5':
#ifdef __ROOT_LOGIN
            *pFlag = TRUE;
            pMsgPkt->NetFn = MSG_NET_FN_ROOT;
            pMsgPkt->cmd = MSG_CMD_ROOT_RENAME_ADMIN;
            pMsgDataRenameUser = (MsgDataRenameUser_t *)pMsgPkt->data;
            pMsgDataRenameUser->VerifyData.UserID = LOGIN_USER_ID;
            memcpy(pMsgDataRenameUser->VerifyData.UserName, LOGIN_USER_NAME, sizeof(LOGIN_USER_NAME)-1);
            memcpy(pMsgDataRenameUser->VerifyData.password, LOGIN_PASSWORD, sizeof(LOGIN_PASSWORD)-1);
            memcpy(pMsgDataRenameUser->OldUserName, "admin01", 7);
            memcpy(pMsgDataRenameUser->NewUserName, "admin02", 7);
            pMsgPkt->DataSize = sizeof(MsgDataRenameUser_t);
            pMsgPkt->DataCRC = CRC32_calculate(&pMsgPkt->data, sizeof(MsgDataRenameUser_t));
            pMsgPkt->PayloadCRC = CRC16_calculate(pMsgPkt, MSG_PALYPOAD_CRC_SIZE);
#else
            pMsgPkt->cmd = MSG_CMD_DO_NOTHING;
            printf("No root login\n");
#endif
            break;
        case '6':
#ifdef __ROOT_LOGIN
            pMsgPkt->NetFn = MSG_NET_FN_ROOT;
            pMsgPkt->cmd = MSG_CMD_ROOT_CLEAR_LOG;
            pMsgDataAddUser = (MsgDataAddUser_t *)pMsgPkt->data;
            pMsgDataAddUser->VerifyData.UserID = LOGIN_USER_ID;
            memcpy(pMsgDataAddUser->VerifyData.UserName, LOGIN_USER_NAME, sizeof(LOGIN_USER_NAME)-1);
            memcpy(pMsgDataAddUser->VerifyData.password, LOGIN_PASSWORD, sizeof(LOGIN_PASSWORD)-1);
            pMsgPkt->DataSize = sizeof(MsgDataAddUser_t);
            pMsgPkt->DataCRC = CRC32_calculate(&pMsgPkt->data, sizeof(MsgDataAddUser_t));
            pMsgPkt->PayloadCRC = CRC16_calculate(pMsgPkt, MSG_PALYPOAD_CRC_SIZE);
#else
            printf("No root login\n");
#endif
            break;
        case '7':
#ifdef __ROOT_LOGIN
            pMsgPkt->NetFn = MSG_NET_FN_DOWNLOAD;
            pMsgPkt->cmd = MSG_CMD_DOWNLOAD_LOG;
#else
            pMsgPkt->cmd = MSG_CMD_DO_NOTHING;
            printf("No root login\n");
#endif
            break;
        case 'q':
            break;
        case 'h':
            DispHelpInfo();
            break;
        default:
            return STAT_ERR;
            break;
    }

    return STAT_OK;
}

void DispHelpInfo(void)
{
    printf("1: Add admin\n");
    printf("2: Del admin\n");
    printf("3: Add user\n");
    printf("4: Del user\n");
    printf("5: Rename user\n");
    printf("6: Clear log\n");
    printf("7: Download log\n");
}

G_STATUS GetResponse(MsgPkt_t *pMsgPkt, int fd, int timeout)
{
    if(0 > timeout)
        return STAT_ERR;

    if(0 > fd)
        return STAT_ERR;
    
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
        {
            printf("[Get response] "
                "Select function return a negative value\n");
            break;
        }

        if(0 == res)
            return STAT_ERR;
        
        if(!(FD_ISSET(fd, &fds)))
            return STAT_ERR;

        ReadDataLength = read(fd, pMsgPkt, sizeof(MsgPkt_t));
        if(sizeof(MsgPkt_t) != ReadDataLength)
        {
            if(0 < ReadDataLength)
            {
                if(HANDSHAKE_SYMBOL == ((char *)pMsgPkt)[0])
                {
                    printf("Detect handshake symbol and ignore it\n");
                    continue;
                }
            }

            printf("[Get response] "
                "Invalid data length in msg queque, actual length: %d\n", ReadDataLength);
            return STAT_ERR;
        }

        if(MSG_CMD_DO_NOTHING == pMsgPkt->cmd)
            continue;

        break;
    }while(1);
    
    return STAT_OK;
}

G_STATUS GetFileFromServer(MsgPkt_t *pMsgPkt, int fd, int timeout)
{
    if(0 > timeout)
        return STAT_ERR;

    if(0 > fd)
        return STAT_ERR;
    
    char FileName[] = "/root/info.log";
    MsgPkt_t MsgPkt;
    MsgDataDownloadFile_t *pMsgDataDownload;
    MsgDataFileInfo_t *pMsgDataFileInfo;
    MsgPktTransferData_t MsgPktTransferData;
    int DataLength;
    uint16_t crc16;
    uint32_t crc32;
    FILE *fp;
    //char NewFileFd;

    pMsgDataDownload = (MsgDataDownloadFile_t *)&MsgPkt.data;
    pMsgDataFileInfo = (MsgDataFileInfo_t *)&MsgPkt.data;
    
    MsgPkt.NetFn = MSG_NET_FN_DOWNLOAD;
    MsgPkt.cmd = MSG_CMD_DOWNLOAD_LOG;
    MsgPkt.fd = g_ResFd;
    MSG_SET_RES(MsgPkt.flag);
    pMsgDataDownload->VerifyData.UserID = LOGIN_USER_ID;
    memcpy(pMsgDataDownload->VerifyData.UserName, LOGIN_USER_NAME, sizeof(LOGIN_USER_NAME)-1);
    memcpy(pMsgDataDownload->VerifyData.password, LOGIN_PASSWORD, sizeof(LOGIN_PASSWORD)-1);
    memcpy(pMsgDataDownload->FileName, FileName, sizeof(FileName));
    MsgPkt.DataSize = sizeof(MsgDataDownloadFile_t);
    MsgPkt.DataCRC = CRC32_calculate(pMsgDataDownload, sizeof(MsgDataDownloadFile_t));
    MsgPkt.PayloadCRC = CRC16_calculate(&MsgPkt, MSG_PALYPOAD_CRC_SIZE);

    DataLength = write(fd, &MsgPkt, sizeof(MsgPkt_t));
    if(sizeof(MsgPkt_t) != DataLength)
    {
        printf("write(): %s\n", strerror(errno));
        return -1;
    }

    if(STAT_OK != GetResponse(&MsgPkt, fd, 3))
    {
        printf("Server no response\n");
        return -1;
    }
    
    crc16 = CRC16_calculate(&MsgPkt, MSG_PALYPOAD_CRC_SIZE);
    if(crc16 != MsgPkt.PayloadCRC)
    {
        printf("Payload CRC verify error\n");
        return -1;
    }

    crc32 = CRC32_calculate(pMsgDataFileInfo, sizeof(MsgDataFileInfo_t));
    if(crc32 != MsgPkt.DataCRC)
    {
        printf("Data CRC verify error\n");
        return -1;
    }
    
    printf("FileName=%s\n", pMsgDataFileInfo->FileName);
    printf("FileSize=%ld\n", pMsgDataFileInfo->FileSize);
    printf("FilePointer=%p\n", pMsgDataFileInfo->fp);
    fp = pMsgDataFileInfo->fp;
    
    MsgPktTransferData.NetFn = MSG_NET_FN_CLIENT;
    MsgPktTransferData.cmd = MSG_CMD_CLIENT_TRANSFER_DATA;
    MsgPktTransferData.fp = fp;
    MsgPktTransferData.offset = 0;
    MsgPktTransferData.size = MSG_TRANSFER_DATA_BASE_SIZE;
    MsgPktTransferData.PayloadCRC = CRC16_calculate(&MsgPktTransferData, sizeof(MsgPktTransferData_t)-sizeof(uint16_t));
    printf("PayloadCRC=%d\n", MsgPktTransferData.PayloadCRC);

    DataLength = write(fd, &MsgPktTransferData, sizeof(MsgPktTransferData_t));
    if(sizeof(MsgPktTransferData_t) != DataLength)
    {
        printf("write(): %s\n", strerror(errno));
        return -1;
    }

    if(STAT_OK != GetResponse(&MsgPkt, fd, 3))
    {
        printf("Server no response\n");
        return -1;
    }

    crc16 = CRC16_calculate(&MsgPkt, MSG_PALYPOAD_CRC_SIZE);
    if(crc16 != MsgPkt.PayloadCRC)
    {
        printf("Payload CRC verify error\n");
        return -1;
    }

    crc32 = CRC32_calculate(&MsgPkt.data, MSG_TRANSFER_DATA_BASE_SIZE);
    if(crc32 != MsgPkt.DataCRC)
    {
        printf("Data CRC verify error\n");
        return -1;
    }

    MsgPktTransferData.NetFn = MSG_NET_FN_CLIENT;
    MsgPktTransferData.cmd = MSG_CMD_CLIENT_TRANSFER_END;
    MsgPktTransferData.fp = NULL;
    MsgPktTransferData.offset = 0;
    MsgPktTransferData.size = 0;
    MsgPktTransferData.PayloadCRC = CRC16_calculate(&MsgPktTransferData, sizeof(MsgPktTransferData_t)-sizeof(uint16_t));
    printf("PayloadCRC=%d\n", MsgPktTransferData.PayloadCRC);

    DataLength = write(fd, &MsgPktTransferData, sizeof(MsgPktTransferData_t));
    if(sizeof(MsgPktTransferData_t) != DataLength)
    {
        printf("write(): %s\n", strerror(errno));
        return -1;
    }

    printf("Success\n");

//    NewFileFd = open("file.tmp", O_CREAT | O_RDWR, 0666);
//    if(0 > NewFileFd)
//        return STAT_ERR;

//    close(NewFileFd);
    
    return STAT_OK;
}
