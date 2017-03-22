#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "queue.h"
#include "stream.h"
#include "common.h"
#include "hitinga.h"

/*
 * QUEUE����ѭ�����������һ��fifo buffer
 * ���У�
 * ÿһ��BUFFER�Ľڵ㶼�Ƕ�̬���ɵ�
 * ѭ������ֻ������������Щ��̬���ɵĽڵ�
 *
 * �ṹ����ȫ�����£�
 * ͷָ����ȷ������Ŀ�ʼ
 * putָ������ʾд�����ݵ�λ�ã�����д��ĵ�ǰλ�ã�
 * getָ������ʾ��ȡ���ݵ�λ�ã����Զ�ȡ�ĵ�ǰλ�ã�
 */

static struct htp_queue_s * gqueue=NULL;
static struct htp_queue_s * gput=NULL;
static struct htp_queue_s * gget=NULL;

/*
 * ��ʼ��FIFO
 */
int HTPBQCreate()
{
    gqueue = (struct htp_queue_s*)malloc(sizeof(struct htp_queue_s));
    if(gqueue==NULL){
        ERROR_PRINTF("Malloc out of memory\n");
        return -1;
    }
    memset(gqueue,0,sizeof(struct htp_queue_s));

    int i=0;
    struct htp_queue_s *p,*q;
    p = gqueue;

    for(i=0;i<HTP_QUEUE_MAX-1;i++){
        q = (struct htp_queue_s*)malloc(sizeof(struct htp_queue_s));
        if(q==NULL){
            ERROR_PRINTF("Malloc out of memory\n");
            return -1;
        }

        memset(q,0,sizeof(struct htp_queue_s));

        p->pnext=q;
        p=q;
    }

    p->pnext=gqueue;
    gput=gqueue;
    gget=gqueue;

    return 0;
}

/*
 * ���FIFO
 */
int HTPBQDestory(void)
{
    if(gqueue==NULL)
      return 0;

    int i=0;
    struct htp_queue_s *p,*q;
    p=gqueue;

    while(p->pnext!=gqueue){
        q=p->pnext;
        p->pnext=q->pnext;

        if(q->frame.pdata!=NULL){
            free(q->frame.pdata);
        }
        free(q);
    }

    if(p->frame.pdata!=NULL){
        free(p->frame.pdata);
    }
    free(p);

    gqueue=NULL;
    gput=NULL;
    gget=NULL;

    return 0;
}

/*
 * ����һ��Frame��FIFO
 */
int HTPBQPut(unsigned char * pframe , int len , int64_t timestamp)
{
    struct htp_stream_frame_s sframe = {
         { 0x47,0x47,0x47,0x47 },
         HTP_ACTION_FRAME,
         { 0,0,0 },
         0,
         0
    };

    if(gqueue==NULL){
        return -1;
    }

    if(gput->frame.pdata!=NULL){
//        printf("no------------>queue\n");
        usleep(20*1000);
        return -2;
    } 

    printf("------------>queue\n");
    gput->frame.timestamp=timestamp;
    gput->frame.size=len+sizeof(struct htp_stream_frame_s);
    gput->frame.pdata = malloc(gput->frame.size);

    sframe.len=len;
    sframe.timestamp=timestamp;

    /*
     * make frame packet head
     */
    memcpy(gput->frame.pdata,&sframe ,sizeof(struct htp_stream_frame_s));
    /*
     * copy frame data
     */
    memcpy(gput->frame.pdata+sizeof(struct htp_stream_frame_s),pframe,len);

    gput=gput->pnext;

    return 0;
}

/*
 * ��FIFO��ȡһ��FRAME
 */
int HTPBQGet(unsigned char ** pdata,int * len)
{
    if(gqueue==NULL){
        printf("queue is null,get\n");
        return -1;
    }

    if(gget->frame.pdata==NULL){
        return -3;
    } 

    *len=gget->frame.size;
    *pdata=gget->frame.pdata;

    return 0;
}

/*
 * ��FIFO���һ��FRAME
 */
int HTPBQRelease()
{
    if(gqueue==NULL){
        return -1;
    }

    if(gget->frame.pdata==NULL){
        return -3;
    } 
    
    printf("release buf---->\n");

    if(gget->frame.pdata!=NULL)
    free(gget->frame.pdata);

    gget->frame.pdata=NULL;
    gget->frame.size=0;
    gget->frame.timestamp=0;

    gget=gget->pnext;

    return 0;
}


