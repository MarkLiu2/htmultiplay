#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "hitinga.h"
#include "server.h"

char * gstatusstring[]={
    "playing",
    "pause",
    "stop",
    NULL
};

char * gcmdstring[]={
    "addplay",
    "destory",
    "getvol",
    "pause",
    "play",
    "seek",
    "setvol",
    "status",
    "stop",
    NULL
};

int msq_id;

int HTPMsgQCreate(void)
{
    int MSG_KEY = 2024;

    /* query and old message and delete it if exists */
    msq_id= msgget(MSG_KEY, IPC_EXCL);
    if(msq_id >= 0) {
        msgctl(msq_id, IPC_RMID, 0);
    } else if (-1 == msq_id) {
        printf("msq_id not exist, continue to next step");
    } else {
        printf("critical error!!!");
        return -1;
    }

    /* create a new message queue*/
    msq_id = msgget(MSG_KEY, IPC_CREAT | 0666);
    if(msq_id < 0) {
        printf("failed to create msq\n");
        return -1;
    }

    int i=0;
    char * p=gcmdstring[i] ;
    do{
        DEBUG_PRINTF("cmd:%d:%s\n",i,p);
        i++;
        p=gcmdstring[i];
    }while(p!=NULL);

    return 0;
}


void HTPGetCommand(int * pcmd, char * param , int * phandle)
{
    int ret;
    struct msgstru msgs;
    
    memset ( &msgs , 0x00 , sizeof(struct msgstru));

    ret = msgrcv(msq_id, &msgs, sizeof(struct msgstru) - sizeof(long), 1, IPC_NOWAIT );
    if( ret > 0 ) {
        if( ret == ENOMSG )
            return -2;

        char *p=msgs.msgtext;
        char *q=strstr(p,":");
        if(q!=NULL){
            *q=0;q++;

            *pcmd=atoi(p);
            DEBUG_PRINTF("cmd:%s\n",gcmdstring[*pcmd]);
            strcpy(param,q);
        }
        else{
            *pcmd=atoi(p);
            DEBUG_PRINTF("cmd:%s\n",gcmdstring[*pcmd]);
        }
    }

    return;
}

void HTPPutCommand(int handle, char * param)
{
    return ;
}


void HTPReqSetVolume(int handle,int vol)
{
//    mixer_set_vol(mixer,vol);
    return;
}


void HTPReqGetVolume(int handle,int vol)
{
    return;
}

void HTPReqStatus(int handle,int status,int pos,int duration)
{
    printf("%s:%d:%d\n" , gstatusstring[status] , pos , duration );

    return;
}



