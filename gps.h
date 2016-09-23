#ifndef __GPS_H_
#define __GPS_H_

#include <stdbool.h>




#define DATA_LEN                0xFF                                    /* test data's len              */
#define GPS_SERIAL_BUF_LEN		1024
#define GPS_INFO_BUF_LEN		80


extern int openSerial(char *cSerialName);
extern bool process_gps_ser_buf(char *gps_ser_buf, int len);
extern void gps_info_parse(void);

#endif/*__GPS_H_*/
