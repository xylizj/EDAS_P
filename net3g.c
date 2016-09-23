#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 
#include <sys/types.h>  
#include <sys/stat.h>  
#include <fcntl.h>

#include <signal.h> 
#include <sys/time.h> 
#include "gpio.h"
#include "common.h"

#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/socket.h>

File_Trans_State fileTransState;


extern struct sockaddr_in g_addr;



int GetTtyACM0Stat( )
{
    char    buffer[BUFSIZ];
    FILE    *read_fp;
    int        chars_read;
    int        ret;
    
    memset( buffer, 0, BUFSIZ );
    read_fp = popen("ls /dev | grep ttyACM0", "r");
    if ( read_fp != NULL ) 
    {
        chars_read = fread(buffer, sizeof(char), BUFSIZ-1, read_fp);
        if (chars_read > 0) 
        {
            ret = 1;
        }
        else
        {
            ret = -1;
        }
        pclose(read_fp);
    }
    else
    {
        ret = -1;
    }

    return ret;
}


int GetNetStat( )
{
    char    buffer[BUFSIZ];
    FILE    *read_fp;
    int        chars_read;
    int        ret;
    
    memset( buffer, 0, BUFSIZ );
    read_fp = popen("ifconfig ppp0 | grep RUNNING", "r");
    if ( read_fp != NULL ) 
    {
        chars_read = fread(buffer, sizeof(char), BUFSIZ-1, read_fp);
        if (chars_read > 0) 
        {
            ret = 1;
        }
        else
        {
            ret = -1;
        }
        pclose(read_fp);
    }
    else
    {
        ret = -1;
    }
    return ret;
}





//char curnetstat= -1;

int detect3g = 0;
int is3gvalid = 0;
int is3gsocket = 0;


char curTransFileName[255];
FILE *fp_curTrans;
int g_fp_curTrans_state = -1;

void HeartBeatReport(int sockfd,const struct sockaddr_in *addr,int len);
void task_heartbeat()
{
	while(1)
	{
		while(g_edas_state.state_3g != 1)
		{
			sleep(1);
		}
		sleep(5);
		if(curMsgstate.state == MsgState_rcv)
		{
			Request_collect(sockfd,&g_addr,sizeof(struct sockaddr_in));
		}
		
		sleep(5);
		HeartBeatReport(sockfd,&g_addr,sizeof(struct sockaddr_in));
		sleep(5);
		HeartBeatReport(sockfd,&g_addr,sizeof(struct sockaddr_in));
		sleep(5);
		HeartBeatReport(sockfd,&g_addr,sizeof(struct sockaddr_in));
		sleep(5);
		HeartBeatReport(sockfd,&g_addr,sizeof(struct sockaddr_in));
	}
}

int powerOffCnt = 0;
int noFileTransCnt = 0;
void task_ChkSndFile()
{
	int i;
	char filename[255];
	int state;

	while(1)
	{		
		sleep(10);		
		state = 0;		
		pthread_mutex_lock(&g_file_mutex); 
		for(i=0;i<filescnt;i++)
		{
			if(filesRecord[i].flag != 2)
			{
				curTransFile = i;
				state = 1;
				break;
			}
		}			
		pthread_mutex_unlock(&g_file_mutex); 

		if(state == 0)
		{
			noFileTransCnt++;
		}
		else
		{
			noFileTransCnt = 0;
		}
		
#if dug_sys > 0 
		printf_va_args("fileTransState:%d,curTransFile :%d,filescnt:%d,state:%d\n",fileTransState,curTransFile,filescnt,state);
#endif 		
		if(g_edas_state.state_T15 == 1)
		{
			powerOffCnt = 0;
		}
		else
		{
			if((noFileTransCnt > 3)&&(fileTransState == IDLE))
			{
				powerOffCnt++;
				sleep(10);
				if(powerOffCnt > 2)
				{
#if dug_sys > 0
 					printf_va_args("noFileTrans_reboot!\n");
#endif
					sleep(1);
					system("reboot");
				}
					
			}
		}
		
		if((fileTransState == IDLE)&&(state == 1))
		{
			if(g_isDownloadCfg)
			{
				continue;
			}

#if dug_sys > 0			
			printf_va_args("fileTransState :%d\n",fileTransState);
#endif
			if((curTransFile >= 0)&&(curTransFile <filescnt))
			{				
				memset(curTransFileName,0,255);
				memcpy(curTransFileName,RECORDDIR,strlen(RECORDDIR));
				memcpy(&curTransFileName[strlen(RECORDDIR)],filesRecord[i].filename,strlen(filesRecord[i].filename));

				memset(cur3gfilename,0,100);
				memcpy(cur3gfilename,filesRecord[i].filename,strlen(filesRecord[i].filename));
				cur3gfilename_len = strlen(filesRecord[i].filename);	
#if dug_sys > 0				
				printf_va_args("curMsgstate.state :%d\n",curMsgstate.state);
#endif				
				if(curMsgstate.state == MsgState_rcv)
				{
					fp_curTrans = fopen(curTransFileName , "rb");
					if(fp_curTrans == NULL)
					{
#if dug_sys > 0
						printf_va_args("fopen failed\n");
#endif
					}
					else
					{
						g_fp_curTrans_state = 1;
					}
					//printf_va_args_en(dug_3g,"1223_filename is :%s\n",curTransFileName);
					Request_upfile(fp_curTrans,sockfd,&g_addr,sizeof(struct sockaddr_in));
					fileTransState = REQUEST_UPFILE;
				}
				
			}
		}

	}	
	
}

void reinit_3g_net()
{
	int port = 5888;
	int pid;

	while(1 != GetTtyACM0Stat())
	{
		sleep(1);
	}
	sleep(15);

	pid = fork();		
	if(pid==0){		
		system("pppd call wcdma");
		perror("execl error");
	}
	else
	{		
		g_edas_state.pppd_pid = pid;		
		sleep(25);
		}
	}

void init_3g_net(void)
{
	//int port = 5888;
	int port;
	int pid;
	int trycnt = 10;
	//struct sockaddr_in addr;

	port = atoi(g_ServerPort);
	while((1 != GetTtyACM0Stat())&&(trycnt--))
	{
		sleep(1);
	}
	sleep(15);

	pid = fork();		
	if(pid==0){		
		//execlp("pppd","pppd","call","wcdma",NULL);
		system("pppd call wcdma");
		//perror("execl error");
	}
	else
	{	
		g_edas_state.pppd_pid = pid;		
		sleep(25);
		sockfd=socket(AF_INET,SOCK_DGRAM,0);
		
		bzero(&g_addr,sizeof(struct sockaddr_in));
		g_addr.sin_family=AF_INET;
		g_addr.sin_port=htons(port);
		if(inet_aton(g_ServerIP,&g_addr.sin_addr)<0){		
			//printf_va_args_en(dug_3g,"inet_aton error\n");
		}
	}
}


