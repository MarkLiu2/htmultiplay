#ifndef HTP_SERVER_H
#define HTP_SERVER_H


/*******************************************************************/
/**   º¯ÊýÉùÃ÷   */


#ifdef    __cplusplus
extern "C"
{
#endif    /*__cplusplus*/

extern char * gstatusstring[];
extern char * gcmdstring[];

int HTPInit();

int HTPDestory();

void HTPGetCommand(int * , char * , int * );

void HTPPutCommand(int , char * );

void HTPReqSetVolume(int handle,int vol);

void HTPReqGetVolume(int handle,int vol);

void HTPReqStatus(int handle,int status,int pos,int duration);

#ifdef    __cplusplus
}
#endif    /*__cplusplus*/
#endif



