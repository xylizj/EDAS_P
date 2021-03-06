#ifndef __SD_H_
#define __SD_H_

#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


#define SD_QUEUE_SIZE 2500

typedef struct
{
  uint8_t		queuefull;				/*queue full,if queue 0 full,bit0=1;if queue 1 full,bit1=1;*/
  uint16_t		max;					/* max data in queue */
  uint16_t		cnt; 				  /*cnt[0] for queue0 data cnt; cnt[1] for queue1 data cnt */
  uint8_t		blocknum;
  uint32_t		chksum;
  uint8_t		*data;//uint8_t		data[SD_QUEUE_SIZE];   /* frame data */
}tSD_buffer;



struct _sd_info{
	unsigned int sd_total;
	unsigned int sd_used;
	unsigned int sd_free;
	unsigned int db_sd_free; 
};

extern struct _sd_info sd_info;

extern void SDBufferInit(void);
extern void Write2SDBuffer(FILE *fp,uint8_t* buffer, uint16_t len);
extern void Write2SDBufferDone(FILE *fp);




#endif/*__SD_H_*/
