#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include "common.h"
#include "rtc.h"


pthread_mutex_t g_file_mutex;
pthread_mutex_t g_rtc_mutex;

volatile unsigned int US_SECOND;
volatile unsigned int US_MILISECOND;

tMsgState curMsgstate;

volatile tEDAS_P_STATE g_sys_info;






char *itoa(int num, char *str, int radix)
{
    char* ptr = str;
    int i;
    int j;

    while (num)
    {
        *ptr++  = '0'+(num % radix);
        num    /= radix;

        if (num < radix)
        {
            *ptr++  = '0'+num;
            *ptr    = '\0';
            break;
        }
    }

    j = ptr - str - 1;

    for (i = 0; i < (ptr - str) / 2; i++)
    {
        int temp = str[i];
        str[i]  = str[j];
        str[j--] = temp;
    }

    return str;
}


int checkdata(unsigned char *buf,unsigned int n)
{
	int i;
	unsigned int sum;
	unsigned char result;
	sum = 0;
	for(i=0;i<n-1;i++)
	{
		sum += buf[i];
	}
	result = 0xFF & sum;
	if(buf[n-1] == result)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}


void creat_FileName(char *ptPath, char *name)
{
	unsigned char year,month,date,hour,minute,second;
	unsigned char rtctime[8];
	int pathlen;
	int caridlen;
	
	pathlen = strlen(ptPath);
	caridlen = strlen(g_sys_info.CAR_ID);
	if(g_sys_info.CAR_ID[caridlen-1] == '\n')
	{
		g_sys_info.CAR_ID[caridlen-1] = 0;
		caridlen--;
	}
	
#if DEBUG_SYS > 0
	printf("strlen(g_sys_info.CAR_ID):%d\n",strlen(g_sys_info.CAR_ID));
	printf("CARDID:%s\n",g_sys_info.CAR_ID);
#endif
	//memset(name,0,255);//20160928 xyl remove
	memcpy(name,RECORDPATH,strlen(RECORDPATH));

	memcpy(&name[pathlen],g_sys_info.CAR_ID,strlen(g_sys_info.CAR_ID));
	name[pathlen+caridlen+0] = '_';
	caridlen += 1;
	
	get_rtctime(rtctime);
	
	year   = rtctime[6];
	month  = 0x1F & rtctime[5];
	date   = 0x3F & rtctime[4];
	hour   = 0x3F & rtctime[2];
	minute = 0x7F & rtctime[1];
	second = 0x7F & rtctime[0];

	name[pathlen+caridlen+0] = '0'+(year >> 4);
	name[pathlen+caridlen+1] = '0'+(year & 0x0F);
	name[pathlen+caridlen+2] = '0'+(month >> 4);
	name[pathlen+caridlen+3] = '0'+(month & 0x0F);
	name[pathlen+caridlen+4] = '0'+(date >> 4);
	name[pathlen+caridlen+5] = '0'+(date & 0x0F);
	name[pathlen+caridlen+6] = '0'+(hour >> 4);
	name[pathlen+caridlen+7] = '0'+(hour & 0x0F);
	name[pathlen+caridlen+8] = '0'+(minute >> 4);
	name[pathlen+caridlen+9] = '0'+(minute & 0x0F);
	name[pathlen+caridlen+10] = '0'+(second >> 4);
	name[pathlen+caridlen+11] = '0'+(second & 0x0F);
	
	name[pathlen+caridlen+12] = '.';
	name[pathlen+caridlen+13] = 'r';
	name[pathlen+caridlen+14] = 'e';
	name[pathlen+caridlen+15] = 's';
	
	name[pathlen+caridlen+16] = 0;
	
}


/*void creatFileName(unsigned char *name)
{
	unsigned char year,month,date,hour,minute,second;
	unsigned char rtctime[8];
	
	get_rtctime(rtctime);
	
	year   = rtctime[6];
	month  = 0x1F & rtctime[5];
	date   = 0x3F & rtctime[4];
	hour   = 0x3F & rtctime[2];
	minute = 0x7F & rtctime[1];
	second = 0x7F & rtctime[0];

	name[0] = '0'+(year >> 4);
	name[1] = '0'+(year & 0x0F);
	name[2] = '0'+(month >> 4);;
	name[3] = '0'+(month & 0x0F);;
	name[4] = '0'+(date >> 4);;
	name[5] = '0'+(date & 0x0F);;
	name[6] = '0'+(hour >> 4);;
	name[7] = '0'+(hour & 0x0F);;
	name[8] = '0'+(minute >> 4);;
	name[9] = '0'+(minute & 0x0F);;
	name[10] = '0'+(second >> 4);;
	name[11] = '0'+(second & 0x0F);;
	name[12] = 0;
	
}*/

unsigned int getSystemTime()
{
	unsigned int stick;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts); 
	stick = ts.tv_sec * 1000 + ts.tv_nsec/1000000;

	return stick;
}


void update_time()
{
	//int stick;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts); 
	
	//stick = ts.tv_sec * 1000 + ts.tv_nsec/1000000;

	US_MILISECOND = ts.tv_nsec/1000000;
	US_SECOND = ts.tv_sec;
}

void chksum(char *buf,int len)
{
	unsigned int i,s;
	s=0;
	for(i=0;i<len;i++)
		s+=*(buf+i);
	*(buf+len) = 0xff&s;
}


void makeKillCommand(char *command,int pid)
{
	char pidstr[10];
	memset(pidstr,0,10);
	memset(command,0,50);
	memcpy(command,"kill ",5);
	itoa(pid,pidstr,10);
	memcpy(&command[5],pidstr,strlen(pidstr));	
}



int getSDstatus(unsigned int *total,unsigned int *used,unsigned int *free)
{
	char buf[200] = {0};
	FILE *sd_fp;
	int read_cnt;
	int ret;
	char *p[10];
	int i;
	
	sd_fp = popen("df /media/sd-mmcblk0p1/  | grep /dev/mmcblk0p1 ", "r");
	if ( sd_fp != NULL ) 
    {
        read_cnt = fread(buf, sizeof(char), 200-1, sd_fp);		
        if (read_cnt > 0) 
        {
			p[0] = strtok(buf," ");
		
			for(i=1;i<6;i++)
			{
				p[i] = strtok(NULL," ");
			}

			*total = atoi(p[1]);
			*used =  atoi(p[2]);
			*free =  atoi(p[3]);
			
            ret = 1;
        }
        else
        {
            ret = -1;
        }
        pclose(sd_fp);
    }
	else
    {
        ret = -1;
    }

    return ret;
}



void init_edas_state(void)
{
	g_sys_info.state_cfg = 0;
	g_sys_info.state_T15 = 1;
	g_sys_info.state_gps = 2;
	g_sys_info.state_k          = 2;
	g_sys_info.state_can0_1939  = 2;
	g_sys_info.state_can0_15765 = 2;
	g_sys_info.state_can1_1939  = 2;
	g_sys_info.state_can1_15765 = 2;
	g_sys_info.state_rtc = 0;
	g_sys_info.state_3g = 0;
	g_sys_info.pppd_pid = -1;
	g_sys_info.net_stat = -1;
	g_sys_info.downloading_cfg = FALSE;
	g_sys_info.CAR_ID = NULL;
	g_sys_info.DeviceID = 0;
	g_sys_info.savegps = 0;
	g_sys_info.t15down_3gtries = 0;
}


void init_user_mutex(void)
{
	int ret;

	ret = pthread_mutex_init(&g_file_mutex, NULL);
    if(ret != 0)  
    {  
        perror("pthread_mutex_init failed\n");  
        exit(EXIT_FAILURE);  
    }
	ret = pthread_mutex_init(&g_rtc_mutex, NULL);  	
    if(ret != 0)  
    {  
        perror("pthread_mutex_init failed\n");  
        exit(EXIT_FAILURE);  
    }
}

void printf_va_args(const char* fmt, ...)
{
	#if DUG_PRINTF_EN
    va_list args;         //定义一个va_list类型的变量，用来储存单个参数
    va_start(args, fmt);  //使args指向可变参数的第一个参数

    vprintf(fmt,args);   //必须用vprintf

    va_end(args);         //结束可变参数的获取
    #endif
}
