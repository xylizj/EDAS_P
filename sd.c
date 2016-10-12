#include <stdlib.h>
#include "common.h"
#include "sd.h"

tSD_buffer SD_buffer;
struct _sd_info sd_info;

void SDBufferInit( void )
{
    SD_buffer.queuefull = 0;
    SD_buffer.cnt = 4;
    SD_buffer.max = SD_QUEUE_SIZE-200;
	SD_buffer.data = (uint8_t *)calloc(1,SD_QUEUE_SIZE);
}

void Write2SDBuffer(FILE *fp,uint8_t* buffer, uint16_t len)
{
	uint16_t i;
	static uint32_t nextupdatetime = 1000;	//if timeout, send data to wince,frequence is 200ms
	if(NULL == SD_buffer.data)
		return;

	//if(SD_buffer.cnt != 4)
	if((SD_buffer.blocknum >= 200)||(getSystemTime() > nextupdatetime)||(len >= SD_buffer.max -SD_buffer.cnt))
	{			
		nextupdatetime = getSystemTime()+200;//if timeout, send data to wince,frequence is 100ms
		SD_buffer.data[0] = 0x5A;
		SD_buffer.data[1] = SD_buffer.blocknum;
		SD_buffer.data[2] = (SD_buffer.cnt-4) & 0xFF;
		SD_buffer.data[3] = (SD_buffer.cnt-4) >> 8;

		SD_buffer.chksum = 0;
		for(i=0;i<SD_buffer.cnt;i++)
		  SD_buffer.chksum += SD_buffer.data[i];	

		SD_buffer.data[SD_buffer.cnt] = 0xFF & SD_buffer.chksum;
		
		/*set USB IN data*/
		//USB_buffer.usbStatus[USB_buffer.current] = BUFFER_FILLED;
		//usbQueueWrite((uint8_t *)&USB_buffer.data[USB_buffer.current][0],&USB_buffer.usbStatus[USB_buffer.current],1+USB_buffer.cnt[USB_buffer.current]);
		fwrite(&SD_buffer.data[0],SD_buffer.cnt+1,1,fp);	
		
		SD_buffer.cnt = 4;
		SD_buffer.blocknum = 0;
		SD_buffer.chksum = 0;	
		//write to sdcard;		
	}
	memcpy(&SD_buffer.data[SD_buffer.cnt],buffer,len);
	SD_buffer.blocknum++;
	SD_buffer.cnt += len;	
}

void Write2SDBufferDone(FILE *fp)
{			
	int i;

	if(NULL == SD_buffer.data)
		return;
	//nextupdatetime = getSystemTime+200;//if timeout, send data to wince,frequence is 100ms
	SD_buffer.data[0] = 0x5A;
	SD_buffer.data[1] = SD_buffer.blocknum;
	SD_buffer.data[2] = (SD_buffer.cnt-4) & 0xFF;
	SD_buffer.data[3] = (SD_buffer.cnt-4) >> 8;

	SD_buffer.chksum = 0;
	for(i=0;i<SD_buffer.cnt;i++)
	  SD_buffer.chksum += SD_buffer.data[i];	

	SD_buffer.data[SD_buffer.cnt] = 0xFF & SD_buffer.chksum;
	
	/*set USB IN data*/
	//USB_buffer.usbStatus[USB_buffer.current] = BUFFER_FILLED;
	//usbQueueWrite((uint8_t *)&USB_buffer.data[USB_buffer.current][0],&USB_buffer.usbStatus[USB_buffer.current],1+USB_buffer.cnt[USB_buffer.current]);
	fwrite(&SD_buffer.data[0],SD_buffer.cnt+1,1,fp);	
	
	SD_buffer.cnt = 4;
	SD_buffer.blocknum = 0;
	SD_buffer.chksum = 0;	

	fclose(fp);	

	free(SD_buffer.data);
	SD_buffer.data = NULL;
}
