#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <sys/select.h>  
#include <errno.h>  

#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "hitinga.h"
#include "stream.h"
#include "queue.h"
#include "common.h"

static int sample_rate;
static int sample_format;
static int sample_channels;
static int sample_size;
static int gstreamstatus;

int client_fd[MAX_CLIENT_NUM];  
#define SERVER_IP "127.0.0.1"  
#define SERVER_PORT 18188  

#define MAX_RECV_LEN 1024  
#define BACK_LOG 8 



static int grunable=0;

unsigned char get_action_rate(int freq)
{
    switch(freq){
        case 8000:
            return HTP_RATE_8000;
        case 16000:
            return HTP_RATE_16000;

        case 22050:
            return HTP_RATE_22050;

        case 44100:
            return HTP_RATE_44100;

        case 48000:
            return HTP_RATE_48000;

        case 96000:
            return HTP_RATE_96000;
        default:
            return HTP_RATE_44100;
    }

    return HTP_RATE_44100;
}

int __StreamSendFrame()
{
    int i=0,ret=0;
    unsigned char * pdata;
    int len=0,len1=0;

    if(gstreamstatus!=HTP_STATUS_PLAY)
      return 0;

    if(HTPBQGet(&pdata,&len1)!=0){
        return 0;
    }

    for (i = 0; i < MAX_CLIENT_NUM; i++){
        ret=0;
        len=len1;
        if (client_fd[i] != -1){
RESEND:
            len=len-ret;
            ret = send(client_fd[i], &pdata[len1-len], len, 0);
            if(ret <= 0) {
                perror ("sendframe");
                close(client_fd[i]);
                client_fd[i]=-1;
            }

            if(ret<len)
              goto RESEND;
        }
    }

    HTPBQRelease();

    return 0;
}

int __StreamProc(void *params)
{
    int sock_fd = -1;  
    int ret = -1;  
    struct sockaddr_in serv_addr;  
    struct sockaddr_in cli_addr;  
    socklen_t serv_addr_len = 0;  
    socklen_t cli_addr_len = 0;  
    char recv_buf[MAX_RECV_LEN];  
    int new_conn_fd = -1;  
    int i = 0;  
    int max_fd = -1;  
    int num = -1;  
    struct timeval timeout;  

    grunable=1;
    gstreamstatus=HTP_STATUS_STOP;

    fd_set read_set;  
    fd_set select_read_set;  

    FD_ZERO(&read_set);  
    FD_ZERO(&select_read_set);  

    for (i = 0; i < MAX_CLIENT_NUM; i++){
        client_fd[i] = -1;  
    }   

    memset(&serv_addr, 0, sizeof(serv_addr));  
    memset(&cli_addr, 0, sizeof(cli_addr));  

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);  
    if (sock_fd < 0){
        perror("Fail to socket");  
        return -1;
    }

    serv_addr.sin_family = AF_INET;  
    serv_addr.sin_port = htons(SERVER_PORT);  
    serv_addr.sin_addr.s_addr = INADDR_ANY ;//inet_addr(SERVER_IP);  

    unsigned int value = 1;  
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&value, sizeof(value)) < 0){
        perror("Fail to setsockopt");  
        return -1;
    }  

    serv_addr_len = sizeof(serv_addr);  
    if (bind(sock_fd, (struct sockaddr*)&serv_addr, serv_addr_len) < 0){
        perror("Fail to bind");  
        return -1;
    }  

    if (listen(sock_fd, BACK_LOG) < 0){
        perror("Fail to listen");  
        return -1;
    }  

    char buf[1024];  
    max_fd = sock_fd;  
    int len;  
    FD_SET(sock_fd, &read_set);  

    while (grunable){
        timeout.tv_sec = 0;  
        timeout.tv_usec = 5*1000;  
        
        max_fd = sock_fd;
        for (i = 0; i < MAX_CLIENT_NUM; i++){
            if (max_fd < client_fd[i]){
                max_fd = client_fd[i];
            }
        }

        select_read_set = read_set;
        ret = select(max_fd + 1, &select_read_set, NULL, NULL, &timeout);
        if (ret == 0){
            /* send frame */
            __StreamSendFrame();	
        }
        else if (ret < 0){
            printf("error occur\n");  
        }
        else{
            if (FD_ISSET(sock_fd, &select_read_set)){
                printf("new client comes\n");  
                len = sizeof(cli_addr);  
                new_conn_fd = accept(sock_fd, (struct sockaddr*)&cli_addr, &len);  
                if (new_conn_fd < 0){
                    perror("Fail to accept");  
                    continue;
                }
                else{
                    for (i = 0; i < MAX_CLIENT_NUM; i++){
                        if (client_fd[i] == -1){
                            client_fd[i] = new_conn_fd;  
                            FD_SET(new_conn_fd, &read_set);  
                            break;  
                        }
                        if (max_fd < new_conn_fd){
                            max_fd = new_conn_fd;  
                        }
                    }
                }
            }
            else{
                for (i = 0; i < MAX_CLIENT_NUM; i++){
                    if (-1 == client_fd[i]){
                        continue;  
                    }  
                    memset(recv_buf, 0, MAX_RECV_LEN);  
                    if (FD_ISSET(client_fd[i], &select_read_set)){
                        num = read(client_fd[i], recv_buf, MAX_RECV_LEN);  
                        if (num < 0){
                            printf("Client(%d) left\n", client_fd[i]);  
                            FD_CLR(client_fd[i], &read_set);  
                            close(client_fd[i]);  
                            client_fd[i] = -1;  
                        }  
                        else if (num > 0){
                            recv_buf[num] = '\0';  
                            printf("Recieve client(%d) data\n", client_fd[i]);  
                            printf("Data: %s\n\n", recv_buf);  
                        }
                        if (num == 0){
                            printf("Client(%d) exit\n", client_fd[i]);  
                            perror("error client");
                            FD_CLR(client_fd[i], &read_set);  
                            close(client_fd[i]);  
                            client_fd[i] = -1;
                        }
                    }
                }
            }
        }  
    }

    grunable=2;

    return 0;
}



int HTPStreamStart(int freq,int format,int channels , int ismp3)
{
    struct htp_stream_start_s sstart={
        { 0x47,0x47,0x47,0x47 },
        HTP_ACTION_START,
        0,
        0,
        0,
    };

    int i=0,ret=0;
    char *buffer;

    sstart.format=(unsigned char )format;
    sstart.rate=get_action_rate(freq);
    sstart.channels=channels;
    buffer=(unsigned char *)&sstart;

    gstreamstatus=HTP_STATUS_PLAY;

    int len = sizeof(struct htp_stream_start_s);

    for (i = 0; i < MAX_CLIENT_NUM; i++){
        debugline;
        if (client_fd[i] != -1){
            debugline;
            ret = send(client_fd[i], buffer, len, 0);
            if(ret<=0) {
                perror ("start");
                close(client_fd[i]);
                client_fd[i]=-1;
            }
        }
    }

    return 0;
}

int HTPStreamStop()
{
    struct htp_stream_common_s sstop={
        { 0x47,0x47,0x47,0x47 },
        HTP_ACTION_STOP,
        { 0,0,0 }
    };

    int i=0,ret;
    int len=sizeof(struct htp_stream_common_s);
    unsigned char * buffer= (unsigned char *)&sstop;

    gstreamstatus=HTP_STATUS_STOP;

    for (i = 0; i < MAX_CLIENT_NUM; i++){
        if (client_fd[i] != -1){
            ret = send(client_fd[i], buffer, len, 0);
            if(ret<=0) {
                perror ("stop");
                close(client_fd[i]);
                client_fd[i]=-1;
            }
        }
    }

    return 0;
}

int HTPStreamPause()
{
    struct htp_stream_common_s spause={
        { 0x47,0x47,0x47,0x47 },
        HTP_ACTION_PAUSE,
        { 0,0,0 }
    };
    int i=0,ret;
    int len=sizeof(struct htp_stream_common_s);
    unsigned char * buffer= (unsigned char *)&spause;

    gstreamstatus=HTP_STATUS_PAUSE;

    for (i = 0; i < MAX_CLIENT_NUM; i++){
        if (client_fd[i] != -1){
            ret = send(client_fd[i], buffer, len, 0);
            if(ret<=0) {
                perror ("pause");
                close(client_fd[i]);
                client_fd[i]=-1;
            }
        }
    }

    return 0;
}


int HTPStreamResume()
{
    struct htp_stream_common_s sresume={
        { 0x47,0x47,0x47,0x47 },
        HTP_ACTION_RESUME,
        { 0,0,0 }
    };

    int i=0,ret;
    int len=sizeof(struct htp_stream_common_s);
    unsigned char * buffer= (unsigned char *)&sresume;

    gstreamstatus=HTP_STATUS_PLAY;

    for (i = 0; i < MAX_CLIENT_NUM; i++){
        if (client_fd[i] != -1){
            ret = send(client_fd[i], buffer, len, 0);
            if(ret<=0) {
                perror ("resume");
                close(client_fd[i]);
                client_fd[i]=-1;
            }
        }
    }

    return 0;
}

int HTPStreamInit()
{
    pthread_attr_t attr;
    pthread_t  threadId;
    int res = 0;

    res = pthread_attr_init(&attr);
    if(res != 0){
        ERROR_PRINTF("Init attr Failed, res:%d.\n", res);
        return -1; 
    }

    res = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if(res != 0){
        ERROR_PRINTF("Set detachstate Failed, res:%d.\n", res);
        return -1; 
    }

    res = pthread_create(&threadId, &attr, (void*)__StreamProc, NULL );
    if(res != 0) {
        ERROR_PRINTF("pthread_create Failed[%s]\n",strerror(errno));
        pthread_attr_destroy(&attr);
        return;
    }

    pthread_attr_destroy(&attr);

    return 0;
}

int HTPStreamDestory()
{
    if( grunable == 2 ) return 0;

    grunable = 0 ;
    do {
        if( grunable == 2  ) {
            DEBUG_PRINTF("Exited ----> player\n");
            break;
        }

        usleep(300 * 1000);
    } while(1);

    return 0;
}


