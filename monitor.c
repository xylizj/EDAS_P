#include <unistd.h>
#include <stdlib.h>

#include "common.h"


void monitor(void)
{	
	if(g_sys_info.state_T15 == 0)  //if t15 down and 3g cannot reach, reboot system!
	{
		g_sys_info.t15down_3gtries++;
		if(g_sys_info.t15down_3gtries > 20)//20min
		{
#if DEBUG_SYS > 0
			printf_va_args("t15 down and 3g cannot reach!reboot!\n");
#endif
			sleep(1);
			system("reboot");
		}
	}
	else
	{		
		g_sys_info.t15down_3gtries = 0;		
	}
}
