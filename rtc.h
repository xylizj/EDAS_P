
#ifndef __EDAS_RTC_H
#define __EDAS_RTC_H


//#include "iic.h"
#define I2C_SLAVE	0x0703
#define I2C_TENBIT	0x0704

#define I2C_ADDR  0xD0
#define RTC_DATA_LEN 9

int get_rtctime(unsigned char *rx_buf);



#endif

