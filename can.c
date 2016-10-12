#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <linux/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include <string.h>

#include "can.h"
#include "common.h"
#include "readcfg.h"
#include "led.h"





struct _can_struct can_struct[CAN_CHANNEL_MAX];

tCAN_RCV1939_QUEUE can1939buf[2];
tCAN_RCV15765_QUEUE can15765buf;



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


void handle_1939(struct can_frame *fr,int chan)
{
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

#if DEBUG_CAN > 0 
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

void handle_15765(struct can_frame *fr,int chan,int msg_no)
{
	tRXBUF rxbuf;
	uint8_t Lchan;

    Lchan = (chan<<2) + msg_no;
	
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

#if DEBUG_CAN > 0
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
		write(can_struct[0].socket,&snd_obj,sizeof(snd_obj));
	}
	else if(can_logic[logchan].channel == 1)
	{
		write(can_struct[1].socket,&snd_obj,sizeof(snd_obj));
	}
	outq[logchan].cnt--;

	/**/
	//error_code &= ~0x0c; //2011-10-16, lzy, after successful tx, clear tx error flag
	outq[logchan].rp++; 				//2011-10-17, after successful tx, inc rp
	outq[logchan].rp &= CAN_TX_QUEUE_MAX_INDEX;
	led_Tx_blink(5);
#if DEBUG_CAN > 0
	printf_va_args("DirectLogCan[%d]Trans,chann:%d, id:%08x!\n",logchan,can_logic[logchan].channel,snd_obj.can_id);
#endif
	/**/
	return 0; 
}



void up_can0(void)
{		
	int rate_cnt;
	int fd;
	char rate[8][10]={"125000","250000","","500000","","","","1000000"};
	struct ifreq ifr0;
	int ret;
	struct sockaddr_can addr0;	
		
	system("ifconfig can0 down");
	sleep(1);
	rate_cnt = can_channel[0].wBaudrate/125 - 1;		

	if((fd=(open("/sys/devices/platform/FlexCAN.0/bitrate",O_CREAT | O_TRUNC | O_WRONLY,0600)))<0)
	{
		perror("open can0:\n"); 		
	}

	write(fd,rate[rate_cnt],strlen(rate[rate_cnt]));
	close(fd);			
	sleep(1);

	system("ifconfig can0 up");
	sleep(1);
#if DEBUG_CAN > 0 		
	printf_va_args("ifconfig can0 up,rate_cnt:%d,bitrate:%s!\n",rate_cnt,rate[rate_cnt]);
#endif
	can_struct[0].socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	strcpy(ifr0.ifr_name, "can0");
	ret = ioctl(can_struct[0].socket, SIOCGIFINDEX, &ifr0);
	addr0.can_family = PF_CAN;
	addr0.can_ifindex = ifr0.ifr_ifindex;
	ret = bind(can_struct[0].socket, (struct sockaddr *)&addr0, sizeof(addr0));
	if (ret < 0) {
		perror("bind can0 failed");
	}
	ret = setsockopt(can_struct[0].socket, SOL_CAN_RAW, CAN_RAW_FILTER, &canfilter[0], canfcnt[0] * sizeof(canfilter[0][0]));
#if DEBUG_CAN > 0 		
	if (ret < 0) {
		printf_va_args("setsockopt can0 failed");
	}
#endif
}



void up_can1(void)
{
	int rate_cnt;
	int fd;
	char rate[8][10]={"125000","250000","","500000","","","","1000000"};
	struct ifreq ifr1;
	int ret;
	struct sockaddr_can addr1;

	system("ifconfig can1 down");
	sleep(1);
	rate_cnt = can_channel[1].wBaudrate/125 - 1;	

	if((fd=(open("/sys/devices/platform/FlexCAN.1/bitrate",O_CREAT | O_TRUNC | O_WRONLY,0600)))<0)
	{
		perror("open can1:\n"); 		
	}
	else{
	}
	write(fd,rate[rate_cnt],strlen(rate[rate_cnt]));
	close(fd);
	sleep(1);

	system("ifconfig can1 up");
	sleep(1);
#if DEBUG_CAN > 0		
	printf_va_args("ifconfig can1 up,rate_cnt:%d,bitrate:%s!\n",rate_cnt,rate[rate_cnt]);
#endif
	can_struct[1].socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	strcpy(ifr1.ifr_name, "can1");
	ret = ioctl(can_struct[1].socket, SIOCGIFINDEX, &ifr1);
	addr1.can_family = PF_CAN;
	addr1.can_ifindex = ifr1.ifr_ifindex;
	bind(can_struct[1].socket, (struct sockaddr *)&addr1, sizeof(addr1));
	ret = setsockopt(can_struct[1].socket, SOL_CAN_RAW, CAN_RAW_FILTER, &canfilter[1], canfcnt[1] * sizeof(canfilter[1][0]));
}


int ulFlashRxId[3],ulFlashTxId[3];
volatile uint32_t currentlid;
volatile uint32_t currentMSize;
int currenLogicChan;
uint8_t currentLidlen;
uint8_t b15765rcv;
uint8_t I2L[CAN_LOGIC_MAX]; /*indexnum to logic number*/   /*查找I对应具体的逻辑通道值*/



void can_info_process(void)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int m = 0;
	uint8_t buffer[10];

	for(i=0;i<CAN_CHANNEL_MAX;i++)
	{			
		for(j=0;j<can_channel[i].ucLogicChanNum;j++)
		{			
			for(k=0;k<can_channel[i].pt_logic_can[j]->ucSignalNum;k++)
			{
				if(getSystemTime() >= can_channel[i].pt_logic_can[j]->pt_signal[k]->dwNextCommTime)
				{
					ulFlashTxId[i] = can_channel[i].pt_logic_can[j]->dwTesterID;
					ulFlashRxId[i] = can_channel[i].pt_logic_can[j]->dwEcuID;				
					currenLogicChan = can_channel[i].pt_logic_can[j]->ucLogicChanIndex;
					currentMSize = can_channel[i].pt_logic_can[j]->pt_signal[k]->ucMemSize;
					can_channel[i].pt_logic_can[j]->pt_signal[k]->dwNextCommTime = getSystemTime()+can_channel[i].pt_logic_can[j]->pt_signal[k]->wSampleCyc;						
					currentlid = can_channel[i].pt_logic_can[j]->pt_signal[k]->dwLocalID;
					currentLidlen = can_channel[i].pt_logic_can[j]->pt_signal[k]->ucLidLength;
					buffer[0] = can_channel[i].pt_logic_can[j]->pt_signal[k]->ucRdDatSerID;
					b15765rcv = 0;
					switch(can_channel[i].pt_logic_can[j]->pt_signal[k]->ucRdDatSerID)
					{
						case 0x21:	//read data by LID
						case 0x22:	//read data by LID
							switch(can_channel[i].pt_logic_can[j]->pt_signal[k]->ucLidLength)									
							{
								case 1:
									buffer[1] = can_channel[i].pt_logic_can[j]->pt_signal[k]->dwLocalID;
									break;
								case 2:
									buffer[1] = can_channel[i].pt_logic_can[j]->pt_signal[k]->dwLocalID>>8;
									buffer[2] = can_channel[i].pt_logic_can[j]->pt_signal[k]->dwLocalID & 0xFF;
									break;
								default: 
									break;
							}
							//Can_SendIso15765Buff(buffer,can_channel[i].pt_logic_can[j]->pt_signal[k]->ucLidLength+1,(i<<2+j));	
							Can_SendIso15765Buff(buffer,can_channel[i].pt_logic_can[j]->pt_signal[k]->ucLidLength+1,i<<(2+j));//20161012 xyl	
#if DEBUG_CAN > 0
							printf_va_args("CAN_Send15765,ulFlashTxId[%d]:%08x,%02x %02x %02x\n",i,ulFlashTxId[i],buffer[0],buffer[1],buffer[2]);
#endif
							break;
						case 0x23:	//read data by MEM
							switch(can_channel[i].pt_logic_can[j]->pt_signal[k]->ucMemSize) 								
							{
								case 1:
									buffer[1] = can_channel[i].pt_logic_can[j]->pt_signal[k]->dwLocalID;
									buffer[2] = can_channel[i].pt_logic_can[j]->pt_signal[k]->ucMemSize;
									break;
								case 2:
									buffer[1] = can_channel[i].pt_logic_can[j]->pt_signal[k]->dwLocalID>>8;
									buffer[2] = can_channel[i].pt_logic_can[j]->pt_signal[k]->dwLocalID & 0xFF;
									buffer[3] = can_channel[i].pt_logic_can[j]->pt_signal[k]->ucMemSize;
									break;
								case 3:
									buffer[1] = can_channel[i].pt_logic_can[j]->pt_signal[k]->dwLocalID>>16;
									buffer[2] = (can_channel[i].pt_logic_can[j]->pt_signal[k]->dwLocalID>>8) & 0xFF;										
									buffer[3] = can_channel[i].pt_logic_can[j]->pt_signal[k]->dwLocalID & 0xFF;
									buffer[4] = can_channel[i].pt_logic_can[j]->pt_signal[k]->ucMemSize;
									break;
								default: break;
							}
							//Can_SendIso15765Buff(buffer,can_channel[i].pt_logic_can[j]->pt_signal[k]->ucMemSize+2,(i<<2+j));//20161012 xyl	
							Can_SendIso15765Buff(buffer,can_channel[i].pt_logic_can[j]->pt_signal[k]->ucMemSize+2,i<<(2+j));
#if DEBUG_CAN
							printf_va_args("CAN_Send15765,ulFlashTxId[%d]:%08x,:%02x %02x %02x %02x %02x\n",i,ulFlashTxId[i],buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]);
#endif
							break;
					}						
									
					m=500;
					while((!b15765rcv) && m)
					{
						m--;
						usleep(1000);
					}				
					
				}
				else
				{
					usleep(1000);
				}
			}
		}
	}
}	







