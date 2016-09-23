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
#include <string.h>


#include "gpio.h"
#include "common.h"
#include "readcfg.h"
#include "kline.h"



#define DATA_LEN                0xFF 
uint16_t kline_WorkingMode;

KLINE_INIT_STATE KlineInitState[2];  


uint8_t  ucKlineFastInitCnt = 0;
uint8_t  kline_UsbCmd_Status = 0;  
uint8_t  kline_UsbBuff[KLINE_RX_BUF_SIZE];
uint16_t kline_UsbDataLength;

KLINE_TX_STATE_TYPE kline_TxState;
KLINE_RX_STATE_TYPE kline_RxState;
uint8_t kline_TxBuf[KLINE_TX_BUF_SIZE];
uint8_t kline_RxBuf[KLINE_RX_BUF_SIZE];
uint8_t kline_Status;

static uint16_t kline_RxErrCnt;

uint16_t kline_WorkingMode;

uint8_t bKlineTxReady;
uint32_t kline_BaudRate;   //2011-09-30

uint32_t	kline_P4;       //?????, ???? 0 - 20ms
uint32_t	kline_P1Max;    //?????, ???? 0 - 20ms
uint32_t  kline_P2Max;    //?????, ???? 0 - 1000ms
uint32_t  kline_P3Max;    //?????, ???? 0 - 5000ms

uint32_t k_rcv_last_time;

tK_RCV15765_QUEUE k15765buf;


//#define B10400 0000014
#define B10400 0010017





int OpenKline(void)
{
	int l_fd_k;

	struct termios opt;
	
	l_fd_k = open("/dev/ttySP0", O_RDWR | O_NOCTTY | O_NDELAY); 
	//fd_k = open("/dev/ttySP0", O_RDWR | O_NOCTTY);
	if(l_fd_k < 0) 
	{
		perror("/dev/ttySP0");
		return -1;
	}
	
	//tcgetattr(fd_k, &opt); 
	if(tcgetattr(l_fd_k, &opt)<0)
	{
#if dug_k > 0
		printf_va_args("kline tcgetattr error\n");
#endif
		return -1;
	}
	cfsetispeed(&opt, B10400);//cfsetispeed(&opt, B9600);
	cfsetospeed(&opt, B10400);//cfsetospeed(&opt, B9600);	

	//opt.c_lflag   &=   ~(ECHO	|	ICANON	 |	 IEXTEN   |   ISIG);
	opt.c_lflag   &=   ~(ICANON | ECHO | ECHOE | ISIG);
	opt.c_iflag   &=   ~(BRKINT   |   ICRNL   |   INPCK   |   ISTRIP   |   IXON);                      
	opt.c_oflag   &=	~(OPOST);	 opt.c_cflag   &=	~(CSIZE   |   PARENB);
	opt.c_cflag &= ~CSIZE;
	opt.c_cflag	|=	 CS8;	 /* 	* 'DATA_LEN' bytes can be read by serial	 */    
	//opt.c_cc[VMIN]	=	DATA_LEN;										   
	//opt.c_cc[VTIME]	=	150;	
	opt.c_cc[VMIN]	=	0;										   
	opt.c_cc[VTIME]	=	0;
	if (tcsetattr(l_fd_k,	 TCSANOW,	&opt)<0) 
	{		  
		return -1;
	}
	
	return l_fd_k;
}





void handl_K_rcvdata(unsigned char *pbuf,int len)
{
	int i;
	if ( ( (pbuf[0]& KLINE_FORMAT_ADDR_MASK) == KLINE_HEADER_MODE_2)|| \
                ( (pbuf[0]& KLINE_FORMAT_ADDR_MASK) == KLINE_HEADER_MODE_3) )
    {
        kline_RxState.headerLength = 3;
    }else
    {
        kline_RxState.headerLength = 1;
    }

	if ((pbuf[0]&KLINE_FORMAT_LENGTH_MASK)!=0)
    {
        kline_RxState.dataLength = (pbuf[0]&KLINE_FORMAT_LENGTH_MASK);      
    }	

	if ((kline_RxState.headerLength%2) == 0)
    {
        kline_RxState.dataLength = pbuf[3];      
    }	

#if(dug_k > 0)
	printf_va_args("kline recv %d:",kline_RxState.dataLength);
	for(i=0;i<len;i++)
	{
		printf_va_args(" %02x ",pbuf[i]);
	}
	printf_va_args("\n");
#endif
}


ssize_t read_kfile(int fd, unsigned char *buf, off_t offset, unsigned int to)
{
	ssize_t cnt;
	ssize_t len;

	/*cnt = 0;
	while(cnt < offset)
	{
		len = read(fd, &buf[cnt], 1);
		if(len > 0)
		{							
			cnt += len;
		}
	}*/
	lseek(fd, offset, SEEK_SET);
	
	cnt = 0; 
	while(to > getSystemTime())
	{
		len = read(fd, &buf[cnt], 1);
		if(len > 0)
		{			
			cnt += len;
			if(cnt >= ((buf[0]&0x3F)+4))
				return cnt;
		}
		usleep(1000);
	}

	return 0;
}




