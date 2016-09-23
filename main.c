#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <string.h>
#include <errno.h>
#include <time.h>


#include "common.h"
#include "queue.h"

#include "sd.h"
#include "boot.h"
#include "readcfg.h"

#include "gpio.h"
#include "rtc.h"

#include "can.h"
#include "kline.h"
#include "net3g.h"

#include "upload_file.h"

#include "task.h"
#include "power_monitor.h"




void net_to_manage(void)
{
	static unsigned int g_TimeoutCnt = 0;

	if(g_Rcv3gCnt > g_LastRcvCnt)
	{
		g_LastRcvCnt = g_Rcv3gCnt;
		g_TimeoutCnt = 0;
		g_3gTryCnt = 0;
	}
	else/*3g timeout*/
	{
		g_TimeoutCnt++;
		if(g_TimeoutCnt >= 2)
		{			
			g_sys_info.net_stat = 0;
			if(g_sys_info.pppd_pid > 0)  //kill pppd
			{
				char killcmd[50];
				g_sys_info.state_3g = 0;									
				makeKillCommand(killcmd,g_sys_info.pppd_pid);
				system(killcmd);
				sleep(1);
				waitpid(g_sys_info.pppd_pid,NULL,0);				
				sleep(5);					
				start3G();
				reinit_3g_net();
				g_sys_info.state_3g = 1;
				g_TimeoutCnt = 0;
			}				
		}
		power_monitor();
	}
}


void main(int argc,char *argv[])
{
	pthread_t id_can,id_k,id_sd,id_k_1ms,id_3g,id_gps,id_CheSndFile,id_wd;
	pthread_t id_udprcv,id_handle_msg,id_checkmsgrx;
	pthread_t id_heartbeat;
	int ret;

	update_boot();
	init_edas_state();
	read_edas_cfg();
	#if	CFG_GPS_ENABLE
	ret=pthread_create(&id_gps,NULL,(void *)task_gps,NULL);
	#endif	
	InitQueue(&msg_queue);
	init_gpio();
	edas_power_on();
	init_adc();	
	
	#if	CFG_3G_ENABLE	
	start3G();
	#endif
	ret=pthread_create(&id_wd,NULL,(void *)task_wd,NULL);
	
	init_user_mutex();
	read_user_cfgset();
	curMsgstate.state = MsgState_rcv;	
	
	#if	CFG_3G_ENABLE
	init_3g_net();
	#endif
	g_sys_info.state_3g = 1;
	ret=pthread_create(&id_can,       NULL,(void *)task_can,        NULL);
	ret=pthread_create(&id_k,         NULL,(void *)task_k,          NULL);
	ret=pthread_create(&id_sd,        NULL,(void *)task_sd,         NULL);
	
	#if	CFG_3G_ENABLE
	ret=pthread_create(&id_heartbeat, NULL,(void *)task_heartbeat,  NULL);
	ret=pthread_create(&id_CheSndFile,NULL,(void *)task_ChkSndFile, NULL);
	ret=pthread_create(&id_udprcv,    NULL,(void *)task_udprcv,     NULL);
	ret=pthread_create(&id_handle_msg,NULL,(void *)task_handle_msg, NULL);
	ret=pthread_create(&id_checkmsgrx,NULL,(void *)task_check_msgrx,NULL);
	#endif	

	while(1)
	{
		static unsigned int g_SysCnt = 0;
				
		led_func();
		usleep(500000);//500ms

		g_SysCnt++;
		if((g_SysCnt % 120) == 0) //500ms * 120 = 1min
		{
			net_to_manage();			
		}
	}

	DestroyQueue(&msg_queue);//can not reach here in normal
}
