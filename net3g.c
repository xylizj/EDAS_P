#include <unistd.h>
#include <arpa/inet.h>
#include "string.h"

#include "common.h"
#include "sd.h"
#include "net3g.h"
#include "gps.h"








struct _net3g net3g;




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
	int pid;
	int tries = 10;
	while((1 != GetTtyACM0Stat())&&(tries--))
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
		net3g.sockfd=socket(AF_INET,SOCK_DGRAM,0);
		
		bzero(&net3g.addr,sizeof(struct sockaddr_in));
		net3g.addr.sin_family=AF_INET;
		if(NULL != net3g.ServerPort)
			net3g.addr.sin_port=htons(atoi(net3g.ServerPort));
		if(NULL != net3g.ServerIP){
			if(inet_aton(net3g.ServerIP,&net3g.addr.sin_addr)<0){		
				//printf_va_args_en(DEBUG_3G,"inet_aton error\n");
			}
			free(net3g.ServerIP);
			net3g.ServerIP = NULL;
		}
	}
}





static 
void fill_sendbuf(char *buf, int DataLen)
{
	char time[8];

	buf[0] = 0x6C;
	buf[1] = 0x00;
	memcpy(&buf[2],(void *)&g_sys_info.DeviceID,4);
	memcpy(&buf[6],&DataLen,2);

	memcpy(&buf[8],&time,8);
	memcpy(&buf[16],(void *)(&gps_phy_struct.longitude),8);
	memcpy(&buf[24],(void *)(&gps_phy_struct.latitude),8);
	memcpy(&buf[32],(void *)(&gps_phy_struct.height),8);
	memcpy(&buf[40],(void *)(&gps_phy_struct.speed),8);
	
	memcpy(&buf[48],(void *)(&sd_info.db_sd_free),8);
	memcpy(&buf[56],(void *)(&g_sys_info),sizeof(g_sys_info));
}


static inline
void print_dug_3g(int send_cnt)
{
	printf_va_args("3g_latitude  = %f\n",gps_phy_struct.longitude);
	printf_va_args("3g_longitude = %f\n",gps_phy_struct.latitude);
	printf_va_args("3g_speed	  = %f\n",gps_phy_struct.height);
	printf_va_args("3g_height	  = %f\n",gps_phy_struct.speed);
	printf_va_args("3g_sd_free	= %d\n",sd_info.db_sd_free);
	printf_va_args("snd heartbeak,cnt:%d\n",send_cnt);
}


static
void hb_kline(void)
{
	static int kline_off_cnt = 0;	

	if(g_sys_info.state_k != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_k == 0)
			{
				kline_off_cnt++;
				if(kline_off_cnt > 100)
				{
#if DEBUG_3G > 0
					printf_va_args("kline off! reboot!\n");
#endif
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				kline_off_cnt = 0;
			}
		}
		g_sys_info.state_k = 0; 	
	}
}

static
void hb_can0_1939(void)
{
	static int can0_1939_off_cnt = 0;

	if(g_sys_info.state_can0_1939 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can0_1939 == 0)
			{
				can0_1939_off_cnt++;
				if(can0_1939_off_cnt > 100)
				{
#if DEBUG_SYS > 0
					printf_va_args("can0_1939_break_reboot!\n");
#endif
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can0_1939_off_cnt = 0;
			}
		}
		g_sys_info.state_can0_1939	= 0;
	}
}


static
void hb_can1_1939(void)
{
	static int can1_1939_off_cnt = 0;

	if(g_sys_info.state_can1_1939 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can1_1939 == 0)
			{
				can1_1939_off_cnt++;
				if(can1_1939_off_cnt > 100)
				{
#if DEBUG_SYS > 0
					printf_va_args("can1_1939_break_reboot!\n");
#endif 
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can1_1939_off_cnt = 0;
			}
		}
		g_sys_info.state_can1_1939	= 0;
	}
}

static
void hb_can0_15765(void)
{
	static int can0_15765_off_cnt = 0;

	if(g_sys_info.state_can0_15765 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can0_15765 == 0)
			{
				can0_15765_off_cnt++;
				if(can0_15765_off_cnt > 100)
				{
#if DEBUG_SYS > 0
					printf_va_args("can0_15765_break_reboot!\n");
#endif					
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can0_15765_off_cnt = 0;
			}
		}
		g_sys_info.state_can0_15765 = 0;
	}
}

static
void hb_can1_15765(void)
{
	static int can1_15765_off_cnt = 0;

	if(g_sys_info.state_can1_15765 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can1_15765 == 0)
			{
				can1_15765_off_cnt++;
				if(can1_15765_off_cnt > 100)
				{
#if DEBUG_SYS > 0
					printf_va_args("can1_15765_break_reboot!\n");
#endif 
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can1_15765_off_cnt = 0;
			}
		}
		g_sys_info.state_can1_15765 = 0;
	}
}


void HeartBeatReport(int sockfd,const struct sockaddr_in *addr,int len)
{
	char *buf = NULL;//20160928 xyl //char buf[MAX_SIZE]={0};
	int send_cnt;
	int DataLen = 8*6 + sizeof(g_sys_info);//add edas_state

	buf = (char *)malloc(MAX_SIZE);
	if(NULL == buf)
		return;
	fill_sendbuf(buf, DataLen);

	chksum(buf,DataLen+8);
	send_cnt = sendto(sockfd,buf,DataLen+9,0,(struct sockaddr *)addr,len);
	free(buf);
	buf = NULL;
#if DEBUG_3G > 0
	print_dug_3g(send_cnt);
#endif
	
	hb_kline();
	hb_can0_1939();
	hb_can1_1939();
	hb_can0_15765();
	hb_can1_15765();
	if(g_sys_info.state_gps != 2)
		g_sys_info.state_gps = 0;
	if((g_sys_info.state_T15==1)&&(g_sys_info.savegps == 1))
	{
		//save heartbeat data
	}

	fill_gps_queue();
}
