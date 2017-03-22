#include <stdio.h>
#include <stdlib.h>  
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>  
#include <arpa/inet.h>  
#include <netinet/in.h>  
#include <pthread.h>
#include <alsa/asoundlib.h>

#include "hitinga.h"
#include "common.h"
#include "queue.h"
#include "alsa.h"
#include "mixer.h"

#define PACKET_BUFFER_LEN 1024*8

static int sample_rate;
static int sample_format;
static int sample_channels;
static int sample_ismp3;
static int sample_size;
static int grunable=0;
static int gclientstatus=0;

static struct timeval tsync;
static long timestampsync;

static unsigned char * ghead=NULL;
static unsigned char * gtail=NULL;
static unsigned char * gwrt=NULL;
static unsigned char * gread=NULL;

struct mixer *mixer;

int get_action_freq(unsigned char rate)
{
    switch(rate){
        case HTP_RATE_8000:
            return 8000;
        case HTP_RATE_16000:
            return 16000;

        case HTP_RATE_22050:
            return 22050;

        case HTP_RATE_44100:
            return 44100;

        case HTP_RATE_48000:
            return 48000;

        case HTP_RATE_96000:
            return 96000;
        default:
            return 44100;
    }

    return 44100;
}

void __init_packet_buff()
{
    ghead=malloc(PACKET_BUFFER_LEN);
    gtail=ghead+PACKET_BUFFER_LEN;
    gwrt=gread=ghead;

    return;
}

void __free_packet_buff()
{
    free(ghead);
    ghead=gwrt=gread=gtail=NULL;

    return;
}

int __read_packet_buff(int sfd,unsigned char **ppbuff,int needs,int *bClosed)
{
    int ret=0;

    if(needs>gtail-gread){
        int nlen=gwrt-gread;
        memcpy(ghead,gread,nlen);
        gread=ghead;
        gwrt=ghead+nlen;
    }

    int mlen=gtail-gwrt;

    if(mlen>0){
        ret = recv(sfd, gwrt, mlen, 0);
        if(ret < 0) {
            DEBUG_PRINTF("read start code fail:peer close[%d]\n",ret);
            *bClosed = 1;
            return -1;
        }
        else if(ret == 0) {
            DEBUG_PRINTF("read start code fail:len=0\n");
            *bClosed = 1;
            return -1;
        }

        gwrt=gwrt+ret;
    }

    if(needs<=gwrt-gread){
        *ppbuff=gread;
        gread+=needs;
        return needs;
    }

    return 0;
}

#define get_long_from_net(x) (unsigned long)x

int __OnFrameRecv(int sfd,int * bClosed)
{
    unsigned char * pbuff;
    unsigned char * pframe;

    if(__read_packet_buff(sfd,&pbuff,sizeof(int)+sizeof(long),bClosed)!=1)
      return -1;

    long framelen = get_long_from_net(pbuff); pbuff+=sizeof(int);
    long timestamp = get_long_from_net(pbuff); pbuff+=sizeof(long);
                
    if(__read_packet_buff(sfd,&pframe,framelen,bClosed)!=framelen)
      return -1;

    /*
     * put queue
     */
    HTPBQPut(pframe,framelen,timestamp);

    return 0;
}

int __OnTimeSync(long sec,long usec,long timestamp)
{
    debugline;
    tsync.tv_sec=sec;
    tsync.tv_usec=usec;
    timestampsync=timestamp;

    return 0;
}

int __OnStart(int rate,int format,int channels , int ismp3)
{
    debugline;
    sample_rate = rate;
    sample_format = format;
    sample_channels = channels;
    sample_ismp3=ismp3;

    gclientstatus=HTP_STATUS_PLAY;

    alsa_open(get_action_freq(sample_rate),sample_format,sample_channels,sample_ismp3);

    return 0;
}

int __OnStop()
{
    debugline;
    gclientstatus=HTP_STATUS_STOP;
    alsa_close();

    return 0;
}

int __OnPause()
{
    debugline;
    gclientstatus=HTP_STATUS_PAUSE;
    alsa_close();

    return 0;
}

int __OnResume()
{
    debugline;
    gclientstatus=HTP_STATUS_PAUSE;
    alsa_open(get_action_freq(sample_rate),sample_format,sample_channels,sample_ismp3);

    return 0;
}


int __StreamRecv(int sfd,int *bClosed)
{
    int ret=0;
    unsigned char *pbuff=NULL;

    *bClosed = 0;

HEAD:
    debugline;
    /*
     * read head
     */
    if(__read_packet_buff(sfd,&pbuff,4,bClosed)!=4){
        printf("readpacket err\n");
        return -1;
    }

    debugline;
    if(pbuff[0]!=0x47&&pbuff[1]!=0x47&&pbuff[2]!=0x47&&pbuff[3]!=0x47){
        goto HEAD;
    }

    debugline;
    /*
     * read action
     */
    if(__read_packet_buff(sfd,&pbuff,4,bClosed)!=1){
        printf("aaaa readpacket err\n");
        return -1;
    }

    debugline;

    int action = pbuff[1];

    switch(action){
        case HTP_ACTION_PAUSE:
            ret =__OnPause();
            break;

        case HTP_ACTION_RESUME:
            ret =__OnResume();
            break;

        case HTP_ACTION_STOP:
            ret =__OnStop();
            break;

        case HTP_ACTION_START:
            ret =__OnStart(pbuff[1],pbuff[2],pbuff[3],0);
            break;

        case HTP_ACTION_TIMESYNC:
            {
                if(__read_packet_buff(sfd,&pbuff,3*sizeof(long),bClosed)!=1)
                  return -1;

                long sec = get_long_from_net(pbuff); pbuff+=sizeof(long);
                long usec = get_long_from_net(pbuff); pbuff+=sizeof(long);
                long timestamp = get_long_from_net(pbuff); pbuff+=sizeof(long);
                
                ret=__OnTimeSync(sec,usec,timestamp);
            }
            break;

        case HTP_ACTION_FRAME:
            {
                ret =__OnFrameRecv(sfd,bClosed);
            }
            break;
    }

    return 0;
}

int __StreamProc(void*params)  
{  
    struct sockaddr_in server_addr;  
    server_addr.sin_family = AF_INET;  
    server_addr.sin_port = htons(HTP_MULTI_PLAY_PORT);  

    server_addr.sin_addr.s_addr = inet_addr((char*)params);  
    bzero(&(server_addr.sin_zero), 8);  

    int server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);  
    if(server_sock_fd == -1){
        perror("socket error");  
        gclientstatus = HTP_STATUS_DESTORY;
        return -1;  
    }

    if(connect(server_sock_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) != 0){
        gclientstatus = HTP_STATUS_DESTORY;
        close(server_sock_fd);
        perror("socket connect");
        return -2;
    } 

    fd_set client_fd_set;  
    struct timeval tv;  

    grunable=1;
    while(grunable==1){
        tv.tv_sec = 2;  
        tv.tv_usec = 10*1000;  

        FD_ZERO(&client_fd_set);  
        FD_SET(server_sock_fd, &client_fd_set);  

        int ret = select(server_sock_fd + 1, &client_fd_set, NULL, NULL, &tv);  
        if(ret < 0 ){
            perror("select");
            continue;
        }

        if(FD_ISSET(server_sock_fd, &client_fd_set)){
            int bClosed;
            __StreamRecv(server_sock_fd,&bClosed);
            if(bClosed==1){
                perror("exit");
                break;
            }
        }
    }

    close(server_sock_fd);
    grunable=0;
    gclientstatus = HTP_STATUS_DESTORY;

    return 0;  
}


int HTPCStreamInit(char * sip)
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

    res = pthread_create(&threadId, &attr, (void*)__StreamProc, (void*)sip );
    if(res != 0) {
        ERROR_PRINTF("pthread_create Failed\n");
        pthread_attr_destroy(&attr);
        return -1;
    }

    pthread_attr_destroy(&attr);

    return 0;
}

int HTPCStreamDestory()
{
    if( grunable == 0 ) return 0;

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

/*
 * 客户端主线程进行如下工作
 * 缓冲区创建
 * 客户端连接线程创建
 * 进行循环的播放处理
 */
int main(int argc , char ** argv)
{
    int ret=0;
    unsigned char * pdata;

    __init_packet_buff();
    HTPBQCreate();
    HTPCStreamInit("127.0.0.1");

    gclientstatus=HTP_STATUS_STOP;

    do{
        switch(gclientstatus){
            case HTP_STATUS_PLAY:
            {
                int len;
                ret = HTPBQGet( &pdata , &len );
                if(ret == 0){
                    alsa_play(pdata,len);
                    HTPBQRelease();
                }
            }
            break;

            case HTP_STATUS_DESTORY:
            goto EXIT;

            case HTP_STATUS_PAUSE:
            case HTP_STATUS_STOP:
            default:
            break;
        }

        usleep(10*1000);

    }while(1);

EXIT:

    HTPCStreamDestory();
    HTPBQDestory();
    __free_packet_buff();

    return 0;
}



