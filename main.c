#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "sd.h"
#include "boot.h"
#include "readcfg.h"
#include "queue.h"

#include "gpio.h"
#include "led.h"
#include "rtc.h"
#include "monitor.h"

#include "can.h"
#include "kline.h"

#include "net3g.h"
#include "recfile.h"
#include "upload_file.h"

#include "task.h"




void net_to_manage(void)
{
	static unsigned int to_cnt = 0;

	if(net3g.rcvcnt > net3g.rcvcnt_last)
	{
		net3g.rcvcnt_last = net3g.rcvcnt;
		to_cnt = 0;
		g_sys_info.t15down_3gtries = 0;
	}
	else/*3g timeout*/
	{
		to_cnt++;
		if(to_cnt >= 2)
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
				to_cnt = 0;
			}				
			monitor();
		}
		//monitor();
	}
}


int main(int argc,char *argv[])
{
	pthread_t id_can,id_k,id_sd,id_gps,id_ChkSndFile,id_wd;
	pthread_t id_udprcv,id_handle_msg,id_checkmsgrx;
	pthread_t id_heartbeat;
	int ret;

	update_boot();
	init_edas_state();
	read_edas_cfg();
	#if	CFG_GPS_ENABLE
	ret = pthread_create(&id_gps,NULL,(void *)task_gps,NULL);
	#endif	
	InitQueue(&curMsgstate.queue);
	init_gpio();
	edas_power_on();
	init_adc();	
	
	#if	CFG_3G_ENABLE	
	start3G();
	#endif
	ret=pthread_create(&id_wd,NULL,(void *)task_wd,NULL);
	
	init_user_mutex();
	acquire_user_cfgset();
	curMsgstate.state = MsgState_rcv;	
	
	#if	CFG_3G_ENABLE
	init_3g_net();
	#endif
	g_sys_info.state_3g = 1;
	ret=pthread_create(&id_can,       NULL,(void *)task_can,        NULL);
	ret=pthread_create(&id_k,         NULL,(void *)task_k,          NULL);
	ret=pthread_create(&id_sd,        NULL,(void *)task_recfile,    NULL);
	
	#if	CFG_3G_ENABLE
	ret=pthread_create(&id_heartbeat, NULL,(void *)task_heartbeat,  NULL);
	ret=pthread_create(&id_ChkSndFile,NULL,(void *)task_ChkSndFile, NULL);
	ret=pthread_create(&id_udprcv,    NULL,(void *)task_udprcv,     NULL);
	ret=pthread_create(&id_handle_msg,NULL,(void *)task_handle_msg, NULL);
	ret=pthread_create(&id_checkmsgrx,NULL,(void *)task_check_msgrx,NULL);
	#endif	

	while(1)
	{
		static unsigned int systick = 0;
				
		led_off();
		if(g_sys_info.net_stat==1){
			led_usb_blink();
		}
		else{
			edas_led_usbon();
		}
		g_sys_info.state_T15 = is_T15_on();
		usleep(500000);//500ms

		systick ++;
		if((systick % 120) == 0){ //500ms * 120 = 1min
			net_to_manage();			
		}
	}

	DestroyQueue(&curMsgstate.queue);//can not reach here in normal
}
