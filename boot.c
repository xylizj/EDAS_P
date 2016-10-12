#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "boot.h"

int update_boot(void)
{
	FILE *fp;
	char buf[200] = {0};
	
	fp = popen("ls /media/sd-mmcblk0p1/boot/ | grep edas_p","r");
	if ( fp != NULL ) 
	{
		if(fread(buf, sizeof(char), 200-1, fp) > 0)
		{
			remove("/root/edas_p");
			sleep(1);
			system("cp /media/sd-mmcblk0p1/boot/edas_p /root/");
			sleep(1);
			remove("/media/sd-mmcblk0p1/boot/edas_p");
			#if DEBUG_SYS > 0
			printf_va_args("Update EDAS finished! reboot!\n");
			#endif
			sleep(1);			
			system("reboot");
		}

		pclose(fp);		
	}

	return 1;
}
