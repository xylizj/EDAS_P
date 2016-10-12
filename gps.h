#ifndef __GPS_H_
#define __GPS_H_

#include <stdbool.h>



#define DATA_LEN                0xFF                                    /* test data's len              */
#define GPS_SERIAL_BUF_LEN		1024
#define GPS_C_BUF_LEN		80
#define GPS_RCV_QUEUE_SIZE       50

#define GPSFLG_LAT 0x01//latitude
#define GPSFLG_LNG 0x02//longitude
#define GPSFLG_SPD 0x04//speed
#define GPSFLG_HIT 0x08//height




typedef struct
{
  uint16_t     len;               /* message id */
  uint8_t      data[50];          /* 8 x data bytes */
}tRcvGpsData;

typedef struct
{
  uint8_t      wp;                     /* write pointer */
  uint8_t      rp;                     /* read pointer */
  uint8_t      cnt;                    /* frame counter in queue */
  tRcvGpsData      qdata[GPS_RCV_QUEUE_SIZE];   /* frame data */
}tGPS_RCV_QUEUE;

extern tGPS_RCV_QUEUE g_gps_queue;



struct _gps_c_struct_{
	unsigned char c_index;
	unsigned char c_buff[GPS_C_BUF_LEN];
	unsigned char comma_idx;
	unsigned char comma_location[20]; 
};
extern struct _gps_c_struct_ gps_c_struct;

struct _gps_tmp_struct_{
	char tmp_latitude[15];
	char tmp_longitude[15];    
	char tmp_speed[15];
	char tmp_height[15]; 
	
	char tmp_latitude_deg[15];
	char tmp_longitude_deg[15]; 	
	char tmp_latitude_min[15];
	char tmp_longitude_min[15]; 

	char tmp_utctime[15];
	char tmp_utcdate[15]; 
};

struct _gps_phy_struct_{	
	volatile double latitude;
	volatile double longitude;	  
	volatile double speed;
	volatile double height;
};

extern struct _gps_tmp_struct_ gps_tmp_struct;
extern struct _gps_phy_struct_ gps_phy_struct;




extern int openSerial(char *cSerialName);
extern bool process_gps_ser_buf(char *gps_ser_raw_buf, int len);
extern void parse_gps_signal(void);
extern void fill_gps_queue(void);

#endif/*__GPS_H_*/
