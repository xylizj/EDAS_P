#ifndef __NET3G_H_
#define __NET3G_H_

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 
#include <sys/types.h>  
#include <sys/stat.h>  
#include <fcntl.h>

#include <signal.h> 
#include <sys/time.h> 

#define GPS_RCV_QUEUE_SIZE       50

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

extern tGPS_RCV_QUEUE g_gps_info;
extern char cur3gfilename[100];
extern int cur3gfilename_len;
extern struct sockaddr_in g_addr;


extern void init_3g_net(void);
extern void HeartBeatReport(int sockfd,const struct sockaddr_in *addr,int len);




#endif

