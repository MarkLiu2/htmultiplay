#ifndef _HT_QUEUE_IF_H
#define _HT_QUEUE_IF_H

#define HTP_QUEUE_MAX  10


struct htp_frame_s{
    int64_t timestamp;
    int size;
    unsigned char * pdata;
};

struct htp_queue_s{
    struct htp_frame_s frame;
    struct htp_queue_s * pnext;
};


#ifdef    __cplusplus
extern "C"
{
#endif    /*__cplusplus*/

int HTPBQCreate(void);

int HTPBQDestory(void);

int HTPBQPut(unsigned char * pframe , int len , int64_t timestamp);

int HTPBQGet(unsigned char ** pdata,int * len);

int HTPBQRelease();

#ifdef    __cplusplus
}
#endif    /*__cplusplus*/
#endif


