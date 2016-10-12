#include <unistd.h>
#include <fcntl.h> 
#include <termios.h>
//#include <errno.h>   

#include "readcfg.h"
#include "kline.h"



KLINE_INIT_STATE KlineInitState[2];  
tKLINE_CONFIG kline_config[2];
tKLINE_SIGNAL *kline_siganl[50];//tKLINE_SIGNAL kline_siganl[50];
uint8_t k_sig_num = 0;

KLINE_TX_STATE_TYPE kline_TxState;
KLINE_RX_STATE_TYPE kline_RxState;

tK_RCV15765_QUEUE k15765queue;








int OpenKline(void)
{
	int l_fd_k;

	struct termios opt;
	
	l_fd_k = open("/dev/ttySP0", O_RDWR | O_NOCTTY | O_NDELAY); 
	if(l_fd_k < 0) 
	{
		perror("/dev/ttySP0");
		return -1;
	}
	
	if(tcgetattr(l_fd_k, &opt)<0)
	{
#if DEBUG_K > 0
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





void handl_K_rcvdata(unsigned char *pbuf)
{
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
}


ssize_t read_kfile(int fd, unsigned char *buf, off_t offset, unsigned int to)
{
	ssize_t cnt;
	ssize_t len;

	cnt = 0;
	while(cnt < offset)
	{
		len = read(fd, &buf[cnt], 1);
		if(len > 0)
		{							
			cnt += len;
		}
	}
	
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
