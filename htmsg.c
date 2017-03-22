#include <stdio.h>   
#include <sys/types.h>   
#include <sys/ipc.h>   
#include <sys/msg.h>   
#include <errno.h>   
#include <string.h>

#define MSGKEY 2024
  
struct msgstru  
{  
   long msgtype;  
   char msgtext[1280];   
};  
  
int main(int argc , char ** argv)  
{  
  struct msgstru msgs;  
  int msg_type;  
  char str[256];  
  int ret_value;  
  int msqid;  
  int nloops =0;
  int nsecnds =1;
  int msgkey=MSGKEY;
  
  if( argc < 2 )
  {
    printf("%s key words\n" , argv[0] );
    return -1;
  }


  if( argc >=  3 ) 
      nloops = atoi(argv[2]);
  else 
      nloops = 1;

  if ( argc >= 4 ) nsecnds = atoi( argv[3] );
  else nsecnds = 1;

printf("nloops = %d , nsecnds=%d \n" , nloops , nsecnds );
  msqid=msgget(MSGKEY,IPC_EXCL);  /*检查消息队列是否存在*/  
  if(msqid < 0){  
#if 0
    msqid = msgget(MSGKEY,IPC_CREAT|0666);/*创建消息队列*/  
    if(msqid <0){  
#endif
    perror("failed to create msq\n");  
    return -1;  
#if 0
    }  
#endif
  }   
  
  memset(str , 0x0 , sizeof(str));
  strcpy( str , argv[1] );

  int cnt =0;
  while (1){  
    printf("input message to be sent:");  

    memset(&msgs , 0x0 , sizeof(msgs)); 

    //scanf ("%s",str);  

    if ( strcmp( str , "exit" ) == 0 )
	break;

    msgs.msgtype = 1; 
    strcpy(msgs.msgtext, (char *)str);  
    /* 发送消息队列 */  
    printf("size=%d\n" , (int)sizeof(struct msgstru));
    ret_value = msgsnd(msqid,&msgs,sizeof(struct msgstru)-sizeof(long),IPC_NOWAIT);  
    if ( ret_value < 0 ) {  
       printf("msgsnd() write msg failed,errno=%d[%s]\n",errno,strerror(errno));  
    }  

    cnt ++;
    if ( cnt >= nloops ) 
    break;
    sleep(nsecnds);
  } 
#if 0
  msgctl(msqid,IPC_RMID,0); //删除消息队列   
#endif

  return 0;
}



