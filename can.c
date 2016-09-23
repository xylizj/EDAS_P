#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <fcntl.h>

#include <signal.h>
#include <sys/types.h>

#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "can.h"
#include "readcfg.h"
#include "kline.h"
#include "gpio.h"
#include "readcfg.h"
#include "gpio.h"


int s_can0,s_can1;
struct sockaddr_can addr_can0,addr_can1;
struct ifreq ifr_can0,ifr_can1;

tRAWCAN_RCV_QUEUE can0queue,can1queue;

tCAN_RCV1939_QUEUE can1939buf[2];
tCAN_RCV15765_QUEUE can15765buf;

tGPS_RCV_QUEUE bps_buf;


TP_RX_ENGINE_TYPE TpRxState[CAN_LOGIC_MAX];
TP_TX_ENGINE_TYPE TpTxState[CAN_LOGIC_MAX]; 

volatile tCAN_RX_QUEUE inq[CAN_LOGIC_MAX];
volatile tCAN_TX_QUEUE outq[CAN_LOGIC_MAX];
uint16_t uiTxDataLength[CAN_LOGIC_MAX];

uint16_t    uiRxIndex[CAN_LOGIC_MAX];
uint8_t     ucRxSn[CAN_LOGIC_MAX];
uint16_t    uiRxReminder[CAN_LOGIC_MAX];
uint16_t    uiRxTimer[CAN_LOGIC_MAX];

uint16_t    uiTxIndex[CAN_LOGIC_MAX];
uint8_t     ucTxSn[CAN_LOGIC_MAX];
uint16_t    uiTxReminder[CAN_LOGIC_MAX];
uint8_t     ucTxTimer[CAN_LOGIC_MAX];

uint8_t     ucTSmin[CAN_LOGIC_MAX] = {1};
uint8_t     ucBSmax[CAN_LOGIC_MAX] = {0};
uint16_t    fc_count[CAN_LOGIC_MAX] = {0};
uint8_t         USB_Flash_Status[CAN_LOGIC_MAX];
uint16_t uiRxDataLength[CAN_LOGIC_MAX];

uint8_t  ucRxBuff[CAN_LOGIC_MAX][268];


/*set the can filter; when add a new canfilter,canxft_cnt++*/
struct can_filter can0filter[100];
struct can_filter can1filter[100];
int can0ft_cnt = 0;
int can1ft_cnt = 0;

//struct can0_frame,can1_frame,can0_fr,can1_fr,can0_frdup,can1_frdup;
//fd_set can0_rset,can1_rset;

void Can_RxIndication(uint8_t channel)
{
    USB_Flash_Status[channel] = KWP_ANSWER_RECEIVED;
}

void Can_ErrIndication(uint8_t logchan,uint8_t errCode)
{
    USB_Flash_Status[logchan] += errCode;
}
void Can_TxConfirmation(uint8_t logchan,uint8_t result)
{
    if( result == CAN_ERR_TX_OK )
    {
        USB_Flash_Status[logchan] = KWP_WAIT_ANSWER;
    }
    else //if( result == TX_WRONG_DATA )
    {
        USB_Flash_Status[logchan] += result;
    }
    
}


void rawcaninit()
{
	can0queue.wp=0;
	can0queue.rp=0;
	can0queue.cnt=0;
	can1queue.wp=0;
	can1queue.rp=0;
	can1queue.cnt=0;
}

int socketCanInit(int chan)
{
	int ret;

	rawcaninit();
	if(chan == 0)
	{
		s_can0 = socket(PF_CAN,SOCK_RAW,CAN_RAW);
		strcpy(ifr_can0.ifr_name,"can0");
		ret = ioctl(s_can0,SIOCGIFINDEX,&ifr_can0);
		if(ret <0 ){
			perror("ioctl can0 failed");
			return 1;
		}
		addr_can0.can_family = PF_CAN;
		addr_can0.can_ifindex = ifr_can0.ifr_ifindex;
		ret = bind(s_can0,(struct sockaddr *)&addr_can0,sizeof(addr_can0));
		if(ret<0){
			perror("bind can0 failed");
		}
	
	}
	else if(chan =1)
	{
		s_can1 = socket(PF_CAN,SOCK_RAW,CAN_RAW);
		strcpy(ifr_can1.ifr_name,"can1");
		ret = ioctl(s_can1,SIOCGIFINDEX,&ifr_can1);
		if(ret <0 ){
			perror("ioctl can0 failed");
			return 1;
		}
		addr_can1.can_family = PF_CAN;
		addr_can1.can_ifindex = ifr_can1.ifr_ifindex;
		ret = bind(s_can1,(struct sockaddr *)&addr_can1,sizeof(addr_can1));
		if(ret<0){
			perror("bind can1 failed");
		}
	}

}

int CanFilterInit(int chan)
{
	int ret;
	can0filter[0].can_id = 0x2000|CAN_EFF_FLAG;
	can0filter[0].can_mask = 0xFFF;
	can0ft_cnt++;
	ret = setsockopt(s_can0,SOL_CAN_RAW,CAN_RAW_FILTER,&can0filter,can0ft_cnt*sizeof(can0filter));
	if(ret<0)
		perror("setsockopt failed");
	return 1;
}

static void handle_1939(struct can_frame *fr,int chan)
{
	int i;
	unsigned int temp_id;

	if(fr->can_id & 0x80000000)
	{
		temp_id = (fr->can_id & 0x1FFFFFFF) | 0x40000000;
	}

	can1939buf[chan].qdata[can1939buf[chan].wp].data[0] = UP_DAQ_SIGNAL;
	can1939buf[chan].qdata[can1939buf[chan].wp].data[1] = SIGNAL_SAE1939;
	can1939buf[chan].qdata[can1939buf[chan].wp].data[2] = 0xFF & 19;;
	can1939buf[chan].qdata[can1939buf[chan].wp].data[3] = 19>>8;
		
	//memcpy(&can1939buf[chan].qdata[can1939buf[chan].wp].data[4],&(fr->can_id),4);
	memcpy(&can1939buf[chan].qdata[can1939buf[chan].wp].data[4],&temp_id,4);
	
	memcpy(&can1939buf[chan].qdata[can1939buf[chan].wp].data[8],(void*)&US_SECOND,4);
	memcpy(&can1939buf[chan].qdata[can1939buf[chan].wp].data[12],(void*)&US_MILISECOND,2);
	can1939buf[chan].qdata[can1939buf[chan].wp].data[14] = chan<<4;  //物理通道
	memcpy(&can1939buf[chan].qdata[can1939buf[chan].wp].data[15],&fr->data[0],8);
	can1939buf[chan].qdata[can1939buf[chan].wp].len = 23;

#if dug_can > 0 
	printf_va_args("can%drcv1939:",chan);
	for(i=0;i<23;i++)
	{
		printf_va_args("%02x ",can1939buf[chan].qdata[can1939buf[chan].wp].data[i]);
	}
	printf_va_args("\n");
#endif

	can1939buf[chan].wp++;
	if(can1939buf[chan].wp == CAN_RCV1939_QUEUE_SIZE)  can1939buf[chan].wp = 0;    
            can1939buf[chan].cnt++;	
}

static void handle_15765(struct can_frame *fr,int chan,int msg_no)
{
	tRXBUF rxbuf;
	uint8_t Lchan;
	int i;

    Lchan = (chan<<2) + msg_no;
	//My_Printf(dug_can,"15765-chan:%d,msg_no:%d,Lchan:%d,currentlid:0x%08x\n",chan,msg_no,Lchan,currentlid);
	
    (void)memset(&rxbuf,0, sizeof(rxbuf));
    rxbuf.id.l=fr->can_id;
    rxbuf.dl=(uint8_t)fr->can_dlc; 
    (void)memcpy((void*)&rxbuf.data[0],(const void *)&fr->data[0],8);         
    (void)memcpy((void*)&inq[Lchan].data[inq[Lchan].wp], (const void *)&rxbuf, 16);

    inq[Lchan].data[inq[Lchan].wp].dl = (byte)(inq[Lchan].data[inq[Lchan].wp].dl&0x0f);
    
    if( Rx_ISO15765_Frame(fr,Lchan) )
    {
        TpRxState[Lchan] = TP_RX_IDLE;
    }
    inq[Lchan].wp++;
    if(inq[Lchan].wp == CAN_RX_QUEUE_SIZE) inq[Lchan].wp = 0;
    inq[Lchan].cnt++;	
    if(USB_Flash_Status[Lchan] == KWP_ANSWER_RECEIVED)
    {  
        /*
                *    received a can 15765 frame, send to pc via USB
                */
        
		if(ucRxBuff[Lchan][0] == 0x7F) goto FLAG_15765;
			
        if(currentMSize)
		{
			uiRxDataLength[Lchan] = uiRxDataLength[Lchan]-1; 
        }
		else
		{
    		uiRxDataLength[Lchan] = uiRxDataLength[Lchan]-currentLidlen-1; //去掉SID,LID
		}
        can15765buf.qdata[can15765buf.wp].data[0] = UP_DAQ_SIGNAL;
        can15765buf.qdata[can15765buf.wp].data[1] = SIGNAL_DIAG_CAN;
        can15765buf.qdata[can15765buf.wp].data[2] = 0xFF & (uiRxDataLength[Lchan]+13);
        can15765buf.qdata[can15765buf.wp].data[3] = (uiRxDataLength[Lchan]+13)>>8;
        can15765buf.qdata[can15765buf.wp].data[4] = 0xFF & currentlid;
        can15765buf.qdata[can15765buf.wp].data[5] = 0xFF & (currentlid>>8);
        can15765buf.qdata[can15765buf.wp].data[6] = 0xFF & (currentlid>>16);
        can15765buf.qdata[can15765buf.wp].data[7] = 0xFF & (currentlid>>24);

	   	memcpy(&can15765buf.qdata[can15765buf.wp].data[8],(void*)&US_SECOND,4);
		memcpy(&can15765buf.qdata[can15765buf.wp].data[12],(void*)&US_MILISECOND,2);
        
        can15765buf.qdata[can15765buf.wp].data[14] = 0xFF & uiRxDataLength[Lchan];
        can15765buf.qdata[can15765buf.wp].data[15] = uiRxDataLength[Lchan] >> 8;
        can15765buf.qdata[can15765buf.wp].data[16] = currenLogicChan;

		if(currentMSize)
		{
			(void)memcpy(&can15765buf.qdata[can15765buf.wp].data[17],(const void *)(&(ucRxBuff[Lchan][1])), (uint32_t)uiRxDataLength[Lchan]);
        	can15765buf.qdata[can15765buf.wp].len = 17+uiRxDataLength[Lchan];
		}
		else
		{
        	(void)memcpy(&can15765buf.qdata[can15765buf.wp].data[17],(const void *)(&(ucRxBuff[Lchan][currentLidlen+1])), (uint32_t)uiRxDataLength[Lchan]);
        	can15765buf.qdata[can15765buf.wp].len = 17+uiRxDataLength[Lchan];
		}

#if dug_can > 0
			printf_va_args("can%drcv15765-%d:",Lchan);
			for(i=0;i<can15765buf.qdata[can15765buf.wp].len;i++)
			{
				printf_va_args("%02x ",can15765buf.qdata[can15765buf.wp].data[i]);
			}
			printf_va_args("\n");
#endif		

        can15765buf.wp++;
        if(can15765buf.wp == CAN_RCV15765_QUEUE_SIZE)  can15765buf.wp = 0;    
        can15765buf.cnt++;
        
FLAG_15765:
        TpRxState[Lchan] = TP_RX_IDLE;
        inq[Lchan].cnt = 0;
        inq[Lchan].wp = 0;
        inq[Lchan].rp = 0;
        b15765rcv = 1;
    }
    return;

	
}

void task_can0_read()
{
	int ret;
	fd_set rset;
	int i;
	char is15765rcv;
	struct can_frame fr,frdup;

	while(1)
	{
		FD_ZERO(&rset);
		FD_SET(s_can0,&rset);
		ret = read(s_can0,&frdup,sizeof(frdup));
		led_Rx_blink(5);
		upDateTime();	

		if(ret<sizeof(frdup)){
			return;
		}
		if(frdup.can_id & CAN_ERR_FLAG){
			continue;
		}

		is15765rcv = 0;
		for(i=0;i<canid_diag_cnt[0];i++)
		{
			if(frdup.can_id == canid_diag_id[0][i])
			{
				//handle 15765
				if(g_sys_info.state_T15==1)
				{
					handle_15765(&frdup,0,i);
					g_sys_info.state_can0_15765 = 1;
					is15765rcv = 1;
				}
				break;
			}
		}	
		//1939 frame
		if((g_sys_info.state_T15==1)&&(is15765rcv == 0))
		{
			handle_1939(&frdup,0);
			g_sys_info.state_can0_1939 = 1;
		}


	}	
}

void task_can1_read()
{
	int ret;
	fd_set rset;
	int i;
	char is15765rcv;
	struct can_frame fr,frdup;

	while(1)
	{
		FD_ZERO(&rset);
		FD_SET(s_can1,&rset);
		ret = read(s_can1,&frdup,sizeof(frdup));
		led_Rx_blink(5);
		upDateTime();	

		if(ret<sizeof(frdup)){
			return;
		}
		if(frdup.can_id & CAN_ERR_FLAG){
			continue;
		}

		is15765rcv = 0;
		for(i=0;i<canid_diag_cnt[1];i++)
		{
			if(frdup.can_id == canid_diag_id[1][i])
			{
				//handle 15765
				if(g_sys_info.state_T15==1)
				{
					handle_15765(&frdup,1,i);
					g_sys_info.state_can1_15765 = 1;
					is15765rcv = 1;
				}
				break;
			}
		}

	
		//1939 frame
		if((g_sys_info.state_T15==1)&&(is15765rcv == 0))
		{
			handle_1939(&frdup,1);
			g_sys_info.state_can1_1939 = 1;
		}


	}	
}
void SendFC(uint8_t channel) 
{
    outq[channel].cnt = 1;         //2009-11-12, lzy, fixed error here
    outq[channel].wp = 0;
    outq[channel].rp = 0;

    outq[channel].data[outq[channel].wp].id.l = ulFlashTxId[channel];
    
    outq[channel].data[outq[channel].wp].dl = 8;
    outq[channel].data[outq[channel].wp].data[0] =  kFlowControlFrame;
    outq[channel].data[outq[channel].wp].data[1] =  0;          //Bsmax
    outq[channel].data[outq[channel].wp].data[2] =  20;         //TSmin, 1->for flash, 20->for diagnostics
    outq[channel].data[outq[channel].wp].data[3] =  0x00;       //TSmin
    outq[channel].data[outq[channel].wp].data[4] =  0x00;       //TSmin
    outq[channel].data[outq[channel].wp].data[5] =  0x00;       //TSmin
    outq[channel].data[outq[channel].wp].data[6] =  0x00;       //TSmin
    outq[channel].data[outq[channel].wp].data[7] =  0x00;       //TSmin
    
//    outq[channel].data[outq[channel].wp].chan = channel; //add for 15765 channel sellcet;
    TpTxState[channel] = TP_TX_FC;
}

uint8_t Rx_ISO15765_Frame(struct can_frame *fr,uint8_t channel)
{
	uint8_t type;
	type = (inq[channel].data[inq[channel].wp].data[0] & 0xF0);

	switch( type ) 
	{
		case kSingleFrame:
			uiRxDataLength[channel] = inq[channel].data[inq[channel].wp].data[0];
			if((uiRxDataLength[channel]>7) || (uiRxDataLength[channel]==0) )
				return 1;
			(void)memcpy(&ucRxBuff[channel][0],(const void *)(&inq[channel].data[inq[channel].wp].data[1]), uiRxDataLength[channel]);
			Can_RxIndication(channel);			
			uiRxTimer[channel] = 0;
			TpRxState[channel] = TP_RX_IDLE;
			return 0;
			//break;
		case kFirstFrame:
			if( (0x0f & (uint8_t)(fr->can_dlc)) != 8 )   //for FF, dl must be 8
				return 1;
			uiRxDataLength[channel] = (uint16_t)((inq[channel].data[inq[channel].wp].data[0] & 0x0f)<<8) + \
			inq[channel].data[inq[channel].wp].data[1];
			if(uiRxDataLength[channel]<8) return 1;
			if(uiRxDataLength[channel]>260) return 1;
			(void)memcpy(&ucRxBuff[channel][0],(const void *)(&inq[channel].data[inq[channel].wp].data[2]), 6);
			uiRxIndex[channel] = 6;
			ucRxSn[channel] = 1;
			uiRxReminder[channel] = uiRxDataLength[channel] - uiRxIndex[channel];
			SendFC(channel);   //we should fix this for receive long frame
			return 0;
			//break;
		case kConsecutiveFrame:
			if( TpRxState[channel] != TP_RX_CF ) return 1;
			if( (inq[channel].data[inq[channel].wp].data[0] & 0x0f) != ucRxSn[channel] ) return 1;
			if( (uiRxDataLength[channel]-uiRxIndex[channel]) < 7 ) 
				memcpy(&ucRxBuff[channel][uiRxIndex[channel]],(const void *)(&inq[channel].data[inq[channel].wp].data[1]),(uiRxDataLength[channel]-uiRxIndex[channel]));
			else
				memcpy(&ucRxBuff[channel][uiRxIndex[channel]],(const void *)(&inq[channel].data[inq[channel].wp].data[1]),7);
			uiRxIndex[channel] += 7;
			if( uiRxIndex[channel] >= uiRxDataLength[channel] ) 
			{
				TpRxState[channel] = TP_RX_IDLE;
				Can_RxIndication(channel);
				uiRxTimer[channel] = 0;
			} 
			else 
			{
				TpRxState[channel] = TP_RX_CF;
				uiRxTimer[channel] = kRxTimeout;
				ucRxSn[channel]++;
				if( ucRxSn[channel] == 16 ) ucRxSn[channel] = 0;
			}
			return 0;
			//break;
			
		case kFlowControlFrame:
			if( TpTxState[channel] != TP_TX_WAIT_FC ) 
			{
				return 0;
			}
			TpTxState[channel] = TP_TX_CF;
			ucBSmax[channel] = inq[channel].data[inq[channel].wp].data[1];
			ucTSmin[channel] = inq[channel].data[inq[channel].wp].data[2];
			ucTxTimer[channel] = ucTSmin[channel];
			return 0;
			//break;
			
		default:
			break;
	}  //switch end
	
	return 1;
}

uint8_t Can_SendIso15765Buff(uint8_t *pData, uint16_t length,uint8_t logchan)
{
	uint16_t segment;
	uint8_t i;
	uint8_t *p;

    TpRxState[logchan] = TP_RX_IDLE;     
    inq[logchan].cnt = 0;
    inq[logchan].wp = 0;
    inq[logchan].rp = 0;

    uiTxDataLength[logchan] = length;
    p = pData;

    if( uiTxDataLength[logchan] > 7 )
    {
        TpTxState[logchan] = TP_TX_REQUEST;
        segment = (unsigned int) ( uiTxDataLength[logchan] / 7 );   //it should be ( (count-6) + 6 )/7
        outq[logchan].data[logchan].id.l=can_logic[logchan].dwTesterID;
            
        outq[logchan].data[0].dl = 8;

        outq[logchan].data[0].data[0] =   kFirstFrame + ( (uiTxDataLength[logchan]>>8)&0x0F );
        outq[logchan].data[0].data[1] =   (BYTE)((uiTxDataLength[logchan])&0xFF);       
        (void)memcpy((void *)&outq[logchan].data[0].data[2], (uint8_t*)p, 6);
        p += 6;  
        ucTxSn[logchan] = 1; 
        ucTxTimer[logchan] = kTxTimeout;
        i = 1;
        while(segment)
        {
        segment-- ;
        outq[logchan].data[i].id.l=can_logic[logchan].dwEcuID;
        outq[logchan].data[i].dl = 8;
//      outq[logchan].data[0].chan = logchan; //0 or 1 ,add by zz
        outq[logchan].data[i].data[0] = (unsigned char)(kConsecutiveFrame | ucTxSn[logchan]);
        (void)memcpy( (void *)&outq[logchan].data[i].data[1], (uint8_t*)p, 7 );
        i++;
        ucTxSn[logchan]++;
        if( ucTxSn[logchan] == 16 ) ucTxSn[logchan] = 0;
        p += 7 ;
        }
        outq[logchan].cnt = i ;

        outq[logchan].rp = 0;
        outq[logchan].wp = i;
    }
    else if( uiTxDataLength[logchan] )
    {
        TpTxState[logchan] = TP_TX_REQUEST;
        outq[logchan].data[0].id.l=can_logic[logchan].dwTesterID;

        outq[logchan].data[0].dl = 8;
//        outq[logchan].data[0].chan = logchan; //0 or 1 ,add by zz
        outq[logchan].data[0].data[0] =   (BYTE)(uiTxDataLength[logchan]&0xFF);       
        (void)memcpy((void *)&outq[logchan].data[0].data[1], p, 7);
        outq[logchan].cnt = 1;
        outq[logchan].rp = 0;
        outq[logchan].wp = 1;     
    } 
    	
	return 0;	
}


uint8_t DirectCanTransmit(uint8_t logchan)
{
	struct can_frame snd_obj;

	snd_obj.can_id = outq[logchan].data[outq[logchan].rp].id.l;
	snd_obj.can_dlc = (byte)(outq[logchan].data[outq[logchan].rp].dl&0x0f);
	(void)memcpy((void *)&(snd_obj.data[0]),(const void *)(&outq[logchan].data[outq[logchan].rp].data[0]),8);

	if(can_logic[logchan].channel == 0)
	{		
		write(s_can0,&snd_obj,sizeof(snd_obj));
	}
	else if(can_logic[logchan].channel == 1)
	{
		write(s_can1,&snd_obj,sizeof(snd_obj));
	}
	outq[logchan].cnt--;

	/**/
	//error_code &= ~0x0c; //2011-10-16, lzy, after successful tx, clear tx error flag
	outq[logchan].rp++; 				//2011-10-17, after successful tx, inc rp
	outq[logchan].rp &= CAN_TX_QUEUE_MAX_INDEX;
	led_Tx_blink(5);
#if dug_can > 0
	printf_va_args("DirectLogCan[%d]Trans,chann:%d, id:%08x!\n",logchan,can_logic[logchan].channel,snd_obj.can_id);
#endif
	/**/
	return 0; 
}











