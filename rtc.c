#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>  
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <termios.h>
#include <errno.h>  
#include <pthread.h>
#include <sys/ioctl.h>

#include "rtc.h"
#include "common.h"	


const char year_monthdays[2][13] = {
	{0,31,28,31,30,31,30,31,31,30,31,30,31},
	{0,31,29,31,30,31,30,31,31,30,31,30,31}
};

char int2bcd(int num)
{
	char result;
	result = 0xF0 & ((num/10)<<4);
	result += (0x0F&(num%10));
	return result;
}

int get_rtctime(unsigned char *rx_buf)
{
	int GiFd;
	unsigned int uiRet;
	int i;
	int waittime;

	unsigned char tx_buf[RTC_DATA_LEN];
	unsigned char addr[2] ;
	addr[0] = 0x00;

	waittime = 20;
	while((g_sys_info.state_rtc != 1)&&(waittime--))
	{
		sleep(1);
	}

	pthread_mutex_lock(&g_rtc_mutex); 
	GiFd = open("/dev/i2c-1", O_RDWR);   
	if(GiFd == -1)
	{
		return -1;
		perror("open rtc\n");
	}

  	uiRet = ioctl(GiFd, I2C_SLAVE, I2C_ADDR >> 1);
	if (uiRet < 0) {
		return -1;
	}

	tx_buf[0] = addr[0];
	for (i = 1; i < RTC_DATA_LEN; i++)
        tx_buf[i] = i;

	write(GiFd, addr, 1);
	read(GiFd, rx_buf, RTC_DATA_LEN - 1);
	close(GiFd);
	pthread_mutex_unlock(&g_rtc_mutex); 
	return 0;
}

int set_rtctime(unsigned char *cst)
{
	int GiFd;
	unsigned int uiRet;
	unsigned char tx_buf[RTC_DATA_LEN];
	unsigned char addr[2];
	addr[0] = 0x00;
	tx_buf[0] = 0x00;

	tx_buf[7] = int2bcd(cst[0]); //year
	tx_buf[6] = int2bcd(cst[1]); //month
	tx_buf[5] = int2bcd(cst[2]); //date
	tx_buf[4] = 0x07 & 1; //day
	tx_buf[3] = int2bcd(cst[3]); tx_buf[3] += 0x80; //hour 
	tx_buf[2] = int2bcd(cst[4]); //minute
	tx_buf[1] = int2bcd(cst[5]); //secend

	pthread_mutex_lock(&g_rtc_mutex); 
	GiFd = open("/dev/i2c-1", O_RDWR);   
	if(GiFd == -1)
	{
		perror("open rtc\n");
		return -1;
	}

  	uiRet = ioctl(GiFd, I2C_SLAVE, I2C_ADDR >> 1);
	if (uiRet < 0) {
		return -1;
	 }
	write(GiFd, tx_buf, 8);
	pthread_mutex_unlock(&g_rtc_mutex);	
	return 1;	
}

void utc2cst(unsigned char *utc,unsigned char *cst)
{
	volatile unsigned char year,mounth,date,hour,minute,second;
	unsigned char isLeapYear;
	
	year   = utc[0];
	mounth = utc[1];
	date   = utc[2];
	hour   = utc[3];
	minute = utc[4];
	second = utc[5];
	
	if(year % 4)
		isLeapYear = 0;
	else
		isLeapYear = 1;	

	hour += 8;

	if(hour >= 24)
	{
		hour -= 24;
		date++;
		
		if(date > year_monthdays[isLeapYear][mounth])
		{
			date = 1;
			mounth++;
			
			if(mounth > 12)
			{
				mounth = 1;
				year++;
			}
		}
	}
	cst[0] = year;
	cst[1] = mounth;
	cst[2] = date;
	cst[3] = hour;
	cst[4] = minute;
	cst[5] = second;	
}









