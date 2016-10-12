#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "gpio.h"



int fd_gpio;
int fd_adc;

void init_gpio(void)
{
	fd_gpio = open("/dev/edas_gpio", O_RDWR);
	if (fd_gpio < 0) {
		perror("open /dev/imx283_gpio");
	}	
	//edas_led_usbon();
}

void init_adc(void)
{
	fd_adc = open("/dev/magic-adc", 0);
	if (fd_adc < 0) {
		close(fd_adc);
	}	
}


int is_T15_on(void)  //t15 on:iRes =2413
{
	int iRes;
	static int t15cnt = 0;
	
	ioctl(fd_adc, 20, &iRes);

	if(t15cnt < 10)  t15cnt++;
		


	if(iRes > 1200)
		iRes = 1;
	else
	{
		if(t15cnt < 8)
		{
			iRes = 1;
		}
		else
			iRes = 0;
	}
	return iRes;
}


void start3G(void)
{
	//edas_3g_off();
	sleep(1);
	edas_3g_work();
	sleep(1);
	edas_3g_start();
	sleep(1);
	edas_3g_restarthi();
	sleep(1);
	edas_3g_restartlo();
	sleep(1);	
}
