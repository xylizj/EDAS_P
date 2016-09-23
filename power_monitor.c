#include "power_monitor.h"
#include "common.h"

unsigned int g_3gTryCnt;

void power_monitor(void)
{
	g_sys_info.state_T15 = is_T15_on();
	if(g_sys_info.state_T15 == 0)  //if t15 down and 3g cannot reach, reboot system!
	{
		g_3gTryCnt++;
		if(g_3gTryCnt > 20)//20min
		{
#if dug_sys > 0
			printf_va_args("t15 down and 3g cannot reach.reboot!\n");
#endif
			sleep(1);
			system("reboot");
		}
	}
	else
	{		
		g_3gTryCnt = 0;		
	}
}
