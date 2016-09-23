#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <limits.h>
#include <asm/ioctls.h>
#include <time.h>
#include <pthread.h>
#include "gpio.h"
#include <sys/ioctl.h>


int fd_gpio;
int fd_adc;

void init_gpio()
{
	fd_gpio = open("/dev/edas_gpio", O_RDWR);
	if (fd_gpio < 0) {
		perror("open /dev/imx283_gpio");
	}	
	//edas_led_usbon();
}

void init_adc()
{
	fd_adc = open("/dev/magic-adc", 0);
	if (fd_adc < 0) {
		close(fd_adc);
	}	
}


int is_T15_on()  //t15 on:iRes =2413
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


void start3G()
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

void led_usb_blink()
{
	static int led_usb_state=0;
	if(led_usb_state)
	{
		edas_led_usbon();
		led_usb_state=0;
	}
	else
	{
		edas_led_usboff();
		led_usb_state=1;
	}
}

void led_k_blink(int num)
{
	static int led_k_state=0;
	static int cnt = 0;
	if(cnt < num) 
	{
		cnt++;
	}
	else
	{
		cnt  = 0;
		if(led_k_state)
		{
			edas_led_kon();
			led_k_state=0;
		}
		else
		{
			edas_led_koff();
			led_k_state=1;
		}
	}
	
}

void led_Rx_blink(int num)
{
	static int led_rx_state=0;
	static int cnt = 0;
	if(cnt < num) 
	{
		cnt++;
	}
	else
	{
		cnt  = 0;
		if(led_rx_state)
		{
			edas_led_rxon();
			led_rx_state=0;
		}
		else
		{
			edas_led_rxoff();
			led_rx_state=1;
		}
	}
	
}
void led_Tx_blink(int num)
{
	static int led_tx_state=0;
	static int cnt = 0;

	if(cnt < num) 
	{
		cnt++;
	}
	else
	{
		cnt=0;
		if(led_tx_state)
		{
			edas_led_txon();
			led_tx_state=0;
		}
		else
		{
			edas_led_txoff();
			led_tx_state=1;
		}
	}
	
}

void task_wd()
{
	while(1)
	{
		edas_wd_on();
		usleep(50000);
		edas_wd_off();
		usleep(50000);
	}
}


