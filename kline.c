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

//#define  B9600	0000015

extern int fd_k;

int OpenKline(void)
{
	//int iFd;
	struct termios opt;

	if(fd_k > 0)
	{
		close(fd_k);
	}

	
	fd_k = open("/dev/ttySP0", O_RDWR | O_NOCTTY | O_NDELAY); 
	//fd_k = open("/dev/ttySP0", O_RDWR | O_NOCTTY);
	if(fd_k < 0) 
	{
		perror("/dev/ttySP0");
		return -1;
	}
	
	//tcgetattr(fd_k, &opt); 
	if(tcgetattr(fd_k, &opt)<0)
	{
#if dug_k > 0
		printf_va_args("kline tcgetattr error\n");
#endif
		return 0;
	}
	cfsetispeed(&opt, B10400);
	cfsetospeed(&opt, B10400);

	//cfsetispeed(&opt, B9600);
	//cfsetospeed(&opt, B9600);

	//opt.c_lflag   &=   ~(ECHO	|	ICANON	 |	 IEXTEN   |   ISIG);
	opt.c_lflag   &=   ~(ICANON | ECHO | ECHOE | ISIG);
	opt.c_iflag   &=   ~(BRKINT   |   ICRNL   |   INPCK   |   ISTRIP   |   IXON);
	//                        
	opt.c_oflag   &=	~(OPOST);	 opt.c_cflag   &=	~(CSIZE   |   PARENB);
	opt.c_cflag &= ~CSIZE;
	opt.c_cflag	|=	 CS8;	 /* 	* 'DATA_LEN' bytes can be read by serial	 */    
	//opt.c_cc[VMIN]	=	DATA_LEN;										   
	//opt.c_cc[VTIME]	=	150;	
	opt.c_cc[VMIN]	=	0;										   
	opt.c_cc[VTIME]	=	0;
	if (tcsetattr(fd_k,	 TCSANOW,	&opt)<0) 
	{		  
		return   -1;
	}    
	return fd_k;

}



int readflag = 0;
uint32_t currentKlinelid;
uint8_t ucLogicKIndex;

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
void task_k()
{	
	int i = 0;
	
	uint8_t kcommand[10];
	uint8_t ktest[10];
	int j,s;
	int d;  //for dug
	int len;
	int kCommandLen;
	int rcvcnt;
	unsigned char pBuf[0xff];
	unsigned char temp[0xff];
	int overtime;
	char tmp[1024];
	int readcnt;
	int Savedatalen;
	int ii;
	
	while(1)
	{		
		if(g_sys_info.state_T15 == 0)
		{
			sleep(1);
			continue;
		}
		if(g_isDownloadCfg)
		{
			sleep(1);
			continue;
		}
		for(i=0;i<siganl_kline_num;i++)
		{			
			if((kline_config[0].bIsValid)&&(g_sys_info.state_T15==1))
			{
				switch(KlineInitState[0])
				{
					case K_INIT_BREAK:
						memset(pBuf,0,0xff);						
						memcpy((void*)kcommand,(const void*)kline_config[0].ucInitFrmData,10);

						readflag = 0;
						OpenKline();
#if (dug_k > 0)
						if(fd_k<0)
						{
							printf_va_args("open kline error\n");
						}
#endif						
						write(fd_k,(void*)kcommand,5);
						readflag = 1;												
						
						kline_TxState.length = 5;						
						rcvcnt = 0;
						overtime = getSystemTime()+2000;
						
						readcnt = 5;
						while(readcnt)
						{
							len = read(fd_k, &pBuf[rcvcnt], 1);
							if(len > 0)
							{							
								readcnt--;
								rcvcnt += len;
							}
						}

						rcvcnt = 0;
						while(overtime > getSystemTime())						
						{
							len = read(fd_k, &pBuf[rcvcnt], 1);							
							if(len >0)
							{
								#if 1							    
								rcvcnt += len;								
								{
									if(rcvcnt >= ((pBuf[0]&0x3F)+4))
										break;
								}
								#endif
							}
							usleep(1000);
						}				
						
						if(pBuf[3] == (0x81+0x40))
						{
							KlineInitState[0] = K_INIT_OK;
							k_rcv_last_time = getSystemTime();
							
						}
						
						if(overtime < getSystemTime())
						{
							KlineInitState[0] = K_TIMEOUT;
							break;
						}						
						break;
					case K_INIT_OK:
						if(k_rcv_last_time + 5000 < getSystemTime())
						{
							KlineInitState[0] = K_TIMEOUT;							
#if (dug_k > 0)
							printf_va_args("K_INIT_OK timeout!\n");
#endif
							break;
						}
						
						if(kline_siganl[i].dwNextTime < getSystemTime())
						{
							currentKlinelid = kline_siganl[i].dwLocalID;
                        	ucLogicKIndex = kline_siganl[i].ucLogicChanIndex;
													
							kcommand[1] = kline_config[0].ucInitFrmData[1];
							kcommand[2] = kline_config[0].ucInitFrmData[2];							
							kcommand[3] = kline_siganl[i].ucRdDatSerID;
							
							switch(kline_siganl[i].ucRdDatSerID)
							{
								case 0x23:
									kcommand[0] = 0x85;									
									kcommand[4] = 0xFF & (kline_siganl[i].dwLocalID >> 16);
									kcommand[5] = 0xFF & (kline_siganl[i].dwLocalID >> 8);
									kcommand[6] = 0xFF & (kline_siganl[i].dwLocalID);		
									kcommand[7] = kline_siganl[i].ucMemSize;
									kCommandLen = 8;
									break;
								case 0x21:									
								case 0x22:
									kcommand[0] = 0x82;
									kcommand[4] = 0xFF & kline_siganl[i].dwLocalID;
									kCommandLen = 5;
									break;
							}							
							
							s = 0;
							for(j=0;j<kCommandLen;j++)
								s += kcommand[j];
							kcommand[kCommandLen] = (uint8_t)(0xFF & s);
							write(fd_k,(void*)kcommand,kCommandLen+1);

#if (dug_k > 0)
							printf_va_args("Ksend:");

							for(d=0;d<kCommandLen+1;d++)
							{
								printf_va_args(" %02x",kcommand[d]);
							}
							printf_va_args("\n");
#endif
														
							kline_siganl[i].dwNextTime = getSystemTime()+kline_siganl[i].wSampleCyc;

							memset(pBuf,0,0xff);
							overtime = getSystemTime()+1000;

							readcnt = kCommandLen+1;							
							
							rcvcnt = 0;
							while(readcnt)
							{
								len = read(fd_k, &pBuf[rcvcnt], 1);
								if(len > 0)
								{							
									readcnt--;
									rcvcnt += len;
								}
							}							
							
							rcvcnt = 0;
							while(overtime > getSystemTime())						
							{
								len = read(fd_k, &pBuf[rcvcnt], 1);							
								if(len >0)
								{
									#if 1							    
									rcvcnt += len;									
									if(rcvcnt >= ((pBuf[0]&0x3F)+4))
										break;
									
									#endif
								}
								usleep(1000);
							}														

							if(overtime < getSystemTime())
							{
								KlineInitState[0] = K_TIMEOUT;
								break;
							}							
							//kline_RxState.dataLength = rcvcnt;
							handl_K_rcvdata(pBuf,rcvcnt);
							
							upDateTime();
							led_k_blink(2);

							//***********************
							//Savedatalen = kline_RxState.dataLength - 2;
							Savedatalen = kline_RxState.dataLength - 1;  //???

							if(kline_siganl[i].ucRdDatSerID != 0x23)
							{
								Savedatalen--;
							}
					        k15765buf.qdata[k15765buf.wp].data[0] = UP_DAQ_SIGNAL;
					        k15765buf.qdata[k15765buf.wp].data[1] = SIGNAL_DIAG_KLINE;
					        k15765buf.qdata[k15765buf.wp].data[2] = 0xFF & (Savedatalen+17-4); 
					        k15765buf.qdata[k15765buf.wp].data[3] = (Savedatalen+17-4) >> 8;
							
					        k15765buf.qdata[k15765buf.wp].data[4] = 0xFF & currentKlinelid;
					        k15765buf.qdata[k15765buf.wp].data[5] = 0xFF & (currentKlinelid>>8);
					        k15765buf.qdata[k15765buf.wp].data[6] = 0xFF & (currentKlinelid>>16);
					        k15765buf.qdata[k15765buf.wp].data[7] = 0xFF & (currentKlinelid>>24);

						   	memcpy(&k15765buf.qdata[k15765buf.wp].data[8], (void*)&US_SECOND,4);
							memcpy(&k15765buf.qdata[k15765buf.wp].data[12], (void*)&US_MILISECOND,2);

							k15765buf.qdata[k15765buf.wp].data[14] = 0xFF & (Savedatalen);
							k15765buf.qdata[k15765buf.wp].data[15] = Savedatalen >> 8; 							
							k15765buf.qdata[k15765buf.wp].data[16] = kline_config[0].ucLogicChanIndex;

							if(kline_siganl[i].ucRdDatSerID == 0x23)
							{
								
								(void)memcpy(&k15765buf.qdata[k15765buf.wp].data[17],(const void *)(&pBuf[kline_RxState.headerLength+1]), Savedatalen);//add kline index
							}
							else
							{
								(void)memcpy(&k15765buf.qdata[k15765buf.wp].data[17],(const void *)(&pBuf[kline_RxState.headerLength+2]), Savedatalen);//add kline index
							}
							
							k_rcv_last_time = getSystemTime();

							k15765buf.qdata[k15765buf.wp].len = Savedatalen + 17;
					        k15765buf.wp++;
					        if(k15765buf.wp == K_RCV15765_QUEUE_SIZE)  k15765buf.wp = 0;    
					        k15765buf.cnt++;
							g_sys_info.state_k = 1;
						//************************								

						}
						
						break;
					case K_TIMEOUT:
						if(fd_k > 0)
						{
							close(fd_k);
						}
						//close(fd_k);
						KlineInitState[0] = K_INIT_BREAK;
						fd_k = 0;
						break;						
				}
			}
		}
		usleep(1000);		
	}
}


