#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 
#include <sys/types.h>  
#include <sys/stat.h>  
#include <fcntl.h>

#include <signal.h> 
#include <sys/time.h> 

#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/socket.h>


#include "gpio.h"
#include "common.h"
#include "recfile.h"
#include "sd.h"
#include "net3g.h"
#include "readcfg.h"


File_Trans_State fileTransState;

tGPS_RCV_QUEUE g_gps_info;
char cur3gfilename[100];
int cur3gfilename_len;
struct sockaddr_in g_addr;



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




void reinit_3g_net(void)
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
		g_sys_info.pppd_pid = pid;		
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
		g_sys_info.pppd_pid = pid;		
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



//char curnetstat= -1;

int detect3g = 0;
int is3gvalid = 0;
int is3gsocket = 0;


char curTransFileName[255];
FILE *fp_curTrans;
int g_fp_curTrans_state = -1;


void HeartBeatReport(int sockfd,const struct sockaddr_in *addr,int len)
{
	static int kline_break_cnt = 0;
	
	static int can0_1939_break_cnt = 0;
	static int can1_1939_break_cnt = 0;
	static int can0_15765_break_cnt = 0;
	static int can1_15765_break_cnt = 0;
	
	char buf[MAX_SIZE]={0};
	char Type = 0x6C;
	char Reserve = 0x00;
	//short DataLen = 8*6;
	short DataLen = 8*7; //add edas_state
	char time[8];
	int cnt;
	unsigned short gpsDataLen;
	
	buf[0] = Type;
	buf[1] = Reserve;
	memcpy(&buf[2],&DeviceID,4);
	memcpy(&buf[6],&DataLen,2);

	memcpy(&buf[8],&time,8);
	memcpy(&buf[16],(void *)(&longitude),8);
	memcpy(&buf[24],(void *)(&latitude),8);
	memcpy(&buf[32],(void *)(&height),8);
	memcpy(&buf[40],(void *)(&speed),8);
	
	memcpy(&buf[48],(void *)(&db_sd_free),8);
	memcpy(&buf[56],(void *)(&g_sys_info),8);
				

#if dug_3g > 0
	printf_va_args("3g_latitude  = %f\n",longitude);
	printf_va_args("3g_longitude = %f\n",latitude);
	printf_va_args("3g_speed	  = %f\n",height);
	printf_va_args("3g_height	  = %f\n",speed);
	printf_va_args("3g_sd_free	= %d\n",db_sd_free);
#endif

	chksum(buf,DataLen+8);
	cnt = sendto(sockfd,buf,DataLen+9,0,(struct sockaddr *)addr,len);
#if dug_3g > 0
	printf_va_args("snd heartbeak,cnt:%d\n",cnt);
#endif
	if(g_sys_info.state_gps != 2)
		g_sys_info.state_gps = 0;
	if(g_sys_info.state_k != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_k == 0)
			{
				kline_break_cnt++;
				if(kline_break_cnt > 100)
				{
#if dug_3g > 0
					printf_va_args("kline_break_reboot!\n");
#endif
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				kline_break_cnt = 0;
			}
		}
		g_sys_info.state_k = 0;
		
	}
	if(g_sys_info.state_can0_1939 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can0_1939 == 0)
			{
				can0_1939_break_cnt++;
				if(can0_1939_break_cnt > 100)
				{
#if dug_sys > 0
					printf_va_args("can0_1939_break_reboot!\n");
#endif
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can0_1939_break_cnt = 0;
			}
		}
		g_sys_info.state_can0_1939	= 0;
	}
	if(g_sys_info.state_can0_15765 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can0_15765 == 0)
			{
				can0_15765_break_cnt++;
				if(can0_15765_break_cnt > 100)
				{
#if dug_sys > 0
					printf_va_args("can0_15765_break_reboot!\n");
#endif					
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can0_15765_break_cnt = 0;
			}
		}
		g_sys_info.state_can0_15765 = 0;
	}
	if(g_sys_info.state_can1_1939 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can1_1939 == 0)
			{
				can1_1939_break_cnt++;
				if(can1_1939_break_cnt > 100)
				{
#if dug_sys > 0
					printf_va_args("can1_1939_break_reboot!\n");
#endif 
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can1_1939_break_cnt = 0;
			}
		}
		g_sys_info.state_can1_1939	= 0;
	}
	if(g_sys_info.state_can1_15765 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can1_15765 == 0)
			{
				can1_15765_break_cnt++;
				if(can1_15765_break_cnt > 100)
				{
#if dug_sys > 0
					printf_va_args("can1_15765_break_reboot!\n");
#endif 
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can1_15765_break_cnt = 0;
			}
		}
		g_sys_info.state_can1_15765 = 0;
	}
	if((g_sys_info.state_T15==1)&&(isSaveGps == 1))
	{
		//save heartbeat data
	}

		gpsDataLen = 32+8;
		DataLen = gpsDataLen+12;
		g_gps_info.qdata[g_gps_info.wp].data[0] = UP_DAQ_SIGNAL;
		g_gps_info.qdata[g_gps_info.wp].data[1] = SIGNAL_GPS;
		g_gps_info.qdata[g_gps_info.wp].data[2] = 0xFF & DataLen;
		g_gps_info.qdata[g_gps_info.wp].data[3] = DataLen>>8;

		g_gps_info.qdata[g_gps_info.wp].data[4] = 0x01;
		g_gps_info.qdata[g_gps_info.wp].data[5] = 0x00;
		g_gps_info.qdata[g_gps_info.wp].data[6] = 0x00;
		g_gps_info.qdata[g_gps_info.wp].data[7] = 0x00;

		memcpy(&g_gps_info.qdata[g_gps_info.wp].data[8],(void*)&US_SECOND,4);
		memcpy(&g_gps_info.qdata[g_gps_info.wp].data[12],(void*)&US_MILISECOND,2);
		
		memcpy(&g_gps_info.qdata[g_gps_info.wp].data[14],&gpsDataLen,2);
		
		memcpy(&g_gps_info.qdata[g_gps_info.wp].data[16],(void*)&longitude,8);
		memcpy(&g_gps_info.qdata[g_gps_info.wp].data[24],(void*)&latitude,8);
		memcpy(&g_gps_info.qdata[g_gps_info.wp].data[32],(void*)&height,8);
		memcpy(&g_gps_info.qdata[g_gps_info.wp].data[40],(void*)&speed,8);			

		g_gps_info.qdata[g_gps_info.wp].len = DataLen+4;
		g_gps_info.wp++;
		if(g_gps_info.wp == GPS_RCV_QUEUE_SIZE)  g_gps_info.wp = 0;	 
		g_gps_info.cnt++;
////////////////////////////////	
}
