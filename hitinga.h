
#ifndef _HTTINGA_H
#define _HTTINGA_H

#include <sys/time.h>

#ifdef CONFIG_HITINGA_SUPPORT
#include "play.h"
#include "mpcif.h"
#include "download.h"
#include "locallist.h"
#include "speech.h"

#include "netif.h"
#include "netwifi.h"
#include "threadmng.h"
#include "timer.h"

#include "devapi.h"
#include "remotesocket.h"
#include "devinfo.h"

#include "soundmsg.h"
#include "statement.h"

#include "neartcp.h"
#include "nearudp.h"
#include "nearparser.h"

#include "config.h"

#include "cdb.h"
#include "led.h"

#else

#define getThreadName() "HTPlayer"
struct msgstru {  
   long msgtype;  
   char msgtext[1280];   
};


#endif


#include <syslog.h>

//#define DEBUG_LINE
//#define DEBUGLOG
//#define ERRORLOG

/*************************************************************************************/
/* DEBUG PRINTF DEFINE 
	    syslog (LOG_INFO, fmt, ## args);\
*************************************************************************************/


#ifdef DEBUGLOG
#define DEBUG_PRINTF(fmt,args...) \
 do {\
		struct timeval tm;\
		gettimeofday(&tm,NULL);\
	    syslog(LOG_DEBUG, "[%ld:%ld] [T:%s] [%s] [%s] [%d]:" fmt,tm.tv_sec,tm.tv_usec,getThreadName(),__FILE__,__FUNCTION__,__LINE__,##args );\
    }while (0)

#else
#define DEBUG_PRINTF(fmt, args...) \
 do {\
		struct timeval tm;\
		gettimeofday(&tm,NULL);\
	    printf( "[%ld:%ld] [T:%s] [%s] [%s] [%d]:" fmt,tm.tv_sec,tm.tv_usec,getThreadName(),__FILE__,__FUNCTION__,__LINE__,##args );\
    }while (0)

#endif



/*************************************************************************************/
/* ERROR PRINTF DEFINE */
/*************************************************************************************/


#ifdef ERRORLOG
#define ERROR_PRINTF(fmt , args...) \
 do {\
		struct timeval tm;\
		gettimeofday(&tm,NULL);\
	    syslog(LOG_ERR, "[%ld:%ld] [T:%s] [%s] [%s] [%d]:" fmt,tm.tv_sec,tm.tv_usec,getThreadName(),__FILE__,__FUNCTION__,__LINE__,##args );\
    }while (0)

#else
#define ERROR_PRINTF(fmt, args...)\
 do {\
		struct timeval tm;\
		gettimeofday(&tm,NULL);\
	    printf( "[%ld:%ld] [T:%s] [%s] [%s] [%d]:" fmt,tm.tv_sec,tm.tv_usec,getThreadName(),__FILE__,__FUNCTION__,__LINE__,##args );\
    }while (0)
#endif


/*************************************************************************************/
/* ASSERT DEFINE */
/*************************************************************************************/

#define myassert(_exp) if((_exp)) {\
    syslog (LOG_ERR, "[T:%s] myassert failed file %s,line %d\n",\
    getThreadName() , __FILE__,__LINE__);\
    exit(0);}


/*************************************************************************************/
/* DEBUG LINE DEFINE */
/*************************************************************************************/


#ifdef DEBUG_LINE
#define debugline  do {\
	struct timeval tm;\
	gettimeofday(&tm,NULL);\
    syslog (LOG_DEBUG, ">>>>>>>>>[%ld:%ld] [T:%s] [%s] [%d]\n",\
    tm.tv_sec , tm.tv_usec,getThreadName() , __FILE__,__LINE__);\
    }while(0)
#else
#define debugline printf("[%s][%s][%d]\n" , __FILE__ , __FUNCTION__ , __LINE__ )
#endif

enum{
    MAIN_PLAY_MODE_ONLINE=0,
    MAIN_PLAY_MODE_LOCAL=1,
    MAIN_PLAY_MODE_AUX=2,

    MAIN_PLAY_MODE_END
};


enum{
    MAIN_STATUS_IDEL=0,
    MAIN_STATUS_RUNNING=1,
    MAIN_STATUS_SPEECH=2,
    MAIN_STATUS_AUX=3,
    MAIN_STATUS_ILLEGAL=4,
    MAIN_STATUS_ALARM=5,
    MAIN_STATUS_STANDBY=6,
    MAIN_STATUS_KILL=7,

    MAIN_STATUS_END
};

#ifdef    __cplusplus
extern "C"
{
#endif    /*__cplusplus*/



#ifdef    __cplusplus
}
#endif    /*__cplusplus*/


#endif



