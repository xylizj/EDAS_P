#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 
#include <sys/types.h>  
#include <sys/stat.h>  
#include <fcntl.h>

#include <signal.h> 
#include <sys/time.h> 
#include "common.h"
#include "boot.h"

int UpdateEdas(void)
{
	FILE *fp;
	char buf[200] = {0};
	
	fp = popen("ls /media/sd-mmcblk0p1/boot/ | grep edas_p","r");
	if ( fp != NULL ) 
	{
		if(fread(buf, sizeof(char), 200-1, fp) > 0)
		{
			remove("/root/edas_p");
			//system("rm /root/edas_p");
			sleep(1);
			system("cp /media/sd-mmcblk0p1/boot/edas_p /root/");
			sleep(1);
			remove("/media/sd-mmcblk0p1/boot/edas_p");
			My_Printf(dug_sys,"upDateEdas_reboot!\n");
			sleep(1);			
			system("reboot");
		}

		pclose(fp);		
	}

	return 1;
}








