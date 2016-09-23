
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <net/if.h>
#include <linux/socket.h>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>


#include "can.h"
#include "gpio.h"
#include "readcfg.h"



char cData[10];
char cTime[8];
char ucTimestampType = 2;
char cProgramName[8] = "EDAS_P";



typedef enum
{
	STATUS_IDLE,
	HEAD_RECEIVING,
	DATA_RECEIVING,
	CHKSUM_RECEIVING,
	STATUS_END
}READ_STATUS;


struct can_filter canfilter[2][100];
int canfcnt[2];

unsigned int  canid_diag_id[2][8];
unsigned int  canid_diag_cnt[2];

int isSaveGps = 0;

unsigned char auth_code[509]={0};

tKWP_USB_HEAD kwp_head_command;
tKLINE_CONFIG kline_config[2];

uint32_t diag_msg_max[2] = {0};
tCAN_COM_CONFIG can_channel[CAN_CHANNEL_MAX];
tCAN_LOGIC_CONFIG can_logic[CAN_LOGIC_MAX];/*0~3:for CAN0;4-7:for CAN1*/
tCAN_SIGNAL can_signal[CAN_SIGNAL_MAX];
uint8_t signal_can_num = 0;
uint8_t can_logic_num = 0;

tKLINE_SIGNAL kline_siganl[50];
uint8_t siganl_kline_num = 0;



//uint32_t CANID_1939[50];
uint8_t  CAN_1939_num = 0;

//tKLINE_CONFIG kline_config[2];
uint8_t kline_channel;
uint8_t bIsGpsValid;

tCAN1939 CAN1939[50];


void read_edas_cfg(void)
{
	FILE *fp_cfg;
	char buff[100];
	int state = 0;

	fp_cfg = fopen("/media/sd-mmcblk0p1/EDASCFG","r");
	if(fp_cfg != NULL)
	{
		state = 0;
		
		while(fgets(buff,100,fp_cfg)!=NULL)
		{
			switch(state)
			{
				case 0:
					if((buff[1]=='I')&&(buff[2]=='P'))   //IP_SET
						state = 1;
					else if((buff[1]=='P')&&(buff[2]=='O')) //PORT_SET
						state = 2;
					else if((buff[1]=='C')&&(buff[2]=='A')) //CAR_ID
						state = 3;
					else if((buff[1]=='D')&&(buff[2]=='E')) //DEVICEID
						state = 4;
					else
						state = 0;
					break;
				case 1:
					memcpy(g_ServerIP,buff,strlen(buff));
					memset(buff,0,100);
					state = 0;
					break;
				case 2:
					memcpy(g_ServerPort,buff,strlen(buff));
					memset(buff,0,100);
					state = 0;
					break;
				case 3:
					memcpy(CAR_ID,buff,strlen(buff));
					memset(buff,0,100);
					printf("\nCARD_ID:%s\n",CAR_ID);
					state = 0;
					break;
				case 4:
					DeviceID = atoi(buff);
					memset(buff,0,100);
					state = 0;
					break;					
			}
		}
		fclose(fp_cfg);
	}
}

char CanIndex2Arraynum(char index)
{
    char hi,lo;
    hi = (index & 0xF0)>>2;
    lo = index & 0x0F;
    return (lo+hi);
}

static int ReadCfg(void)
{
	FILE *fp;
	unsigned int iCount;
	unsigned int i,j,k,ch;
	unsigned int head;
	READ_STATUS state;
	tHEADER header;
	tTYPE_HEADER typeheader;
	unsigned int head_rcv_cnt;
	unsigned int data_rcv_cnt;
	unsigned char ucLogicIndex,arraynum;
	unsigned char cchan;
	int tem_id;
	int cnt_cfg_signal = 0;

	unsigned char channel_can_x;

	char data[500];
	
	fp = fopen(EDASCFGFILE,"rb");
	if(fp == NULL)
	{
		return -1;
	}
	
	fseek(fp,0,SEEK_END);
	iCount = ftell(fp);
	//printf_va_args_en(dug_readcfg,"%d bytes total!\n",iCount);
	rewind(fp);
	state = STATUS_IDLE;
	
	for(i=0;i<iCount;i++)
	{
		ch = fgetc(fp);
		
		switch(state)
		{
			case STATUS_IDLE:
				if(ch==0xA5)
				{
					state = HEAD_RECEIVING;
					head_rcv_cnt = 1;
					header.Fmt = 0xA5;
					header.DataLen = 0;
				}
				break;
			case HEAD_RECEIVING:
				head_rcv_cnt++;
				switch(head_rcv_cnt)
				{
					case 2:
						header.BlockNum = ch;
						break;
					case 3:
						header.DataLen += ch;
						break;
					case 4:
						header.DataLen += (ch<<8);
						if(header.DataLen)
						{
							state = DATA_RECEIVING;
							data_rcv_cnt = 0;
						}
						else
						{
							state = CHKSUM_RECEIVING;
						}
						break;
				}
				break;
			case DATA_RECEIVING:
				data_rcv_cnt++;
				switch(data_rcv_cnt)
				{
					case 1:
						typeheader.Type = ch;
						typeheader.BlockSize = 0;						
						break;
					case 2:
						typeheader.Status = ch;
						break;
					case 3:
						typeheader.BlockSize += ch;
						break;
					case 4:
						typeheader.BlockSize += (ch<<8);
						if(typeheader.BlockSize == 0)
							state = CHKSUM_RECEIVING;
						if(typeheader.BlockSize+4 != header.DataLen)
							state = STATUS_IDLE;
						break;
					default:
						if(data_rcv_cnt <= header.DataLen)		
						{
							data[data_rcv_cnt-5] = ch;	
							if(data_rcv_cnt >= header.DataLen)
								state = CHKSUM_RECEIVING;							
						}
						else
							state = CHKSUM_RECEIVING;
						break;
						
				}
				break;
			case CHKSUM_RECEIVING:  //do not calculate the chksum for simple
				state = STATUS_IDLE;
				
				switch(typeheader.Type)
				{
					case REQ_FREE_SRC://do nothing
						//printf_va_args_en(dug_readcfg,"REQ_FREE_SRC\n");
						break;
					case REQ_CONFIG_COM://do nothing
						canfcnt[0] = 0;
						canfcnt[1] = 0;
						canid_diag_cnt[0]=0;
						canid_diag_cnt[1]=0;

						can1939buf[0].cnt = 0;
						can1939buf[1].cnt = 0;
						can1939buf[0].rp = 0;
						can1939buf[1].rp = 0;
						can1939buf[0].wp = 0;
						can1939buf[1].wp = 0;

						can15765buf.cnt = 0;
						can15765buf.rp = 0;
						can15765buf.wp = 0;
#if dug_readcfg	> 0						
						printf_va_args("REQ_CONFIG_COM\n");
#endif						
						break;
					case DOWN_CONFIG_CAN:
						if(data[0] == 1)  //is valid
						{							
							channel_can_x = typeheader.Status>>4;
							
							if(channel_can_x > 2) //error
							{
								state = STATUS_IDLE;
								break;
							}							
							
							memcpy(&can_channel[channel_can_x],(const void *)&data[0],4);							
#if dug_readcfg	> 0
							printf_va_args("can_channel[]: %d \n",channel_can_x);
							printf_va_args("bIsValid:%d\n",can_channel[channel_can_x].bIsValid);
							printf_va_args("nLogicDiagChanNum:%d\n",can_channel[channel_can_x].nLogicDiagChanNum);
							printf_va_args("wBaudrate:%d\n",can_channel[channel_can_x].wBaudrate);
#endif							
							j=(typeheader.BlockSize-4)/12;
				
							for(i=0;i<j;i++)
							{
								ucLogicIndex = data[12*i+13];	//14-->13							
								arraynum = CanIndex2Arraynum(ucLogicIndex);
								I2L[can_logic_num] = arraynum;								
								memcpy(&can_logic[arraynum],(const void *)&data[12*i+4],12);

								

								if(can_logic[arraynum].dwEcuID & 0x40000000)
								{
									tem_id = (can_logic[arraynum].dwEcuID & 0x1FFFFFFF) | 0x80000000;
									can_logic[arraynum].dwEcuID = tem_id;
								}
								
								if(can_logic[arraynum].dwTesterID & 0x40000000)
								{
									tem_id = (can_logic[arraynum].dwTesterID & 0x1FFFFFFF) | 0x80000000;
									can_logic[arraynum].dwTesterID = tem_id;
								}		
								
								can_channel[channel_can_x].pt_logic_can[can_channel[channel_can_x].ucLogicChanNum]=&can_logic[arraynum];
                        		can_channel[channel_can_x].dwTesterID[can_channel[channel_can_x].ucLogicChanNum] = can_logic[arraynum].dwTesterID;
								can_channel[channel_can_x].dwEcuID[can_channel[channel_can_x].ucLogicChanNum] = can_logic[arraynum].dwEcuID;
                        		can_channel[channel_can_x].ucLogicChanNum++;
                        		can_logic[arraynum].ucSignalNum = 0;
								signal_can_num = 0;
                        		can_logic[arraynum].channel = channel_can_x;                        
                        		can_logic_num++;	

								
								if(can_logic[arraynum].dwEcuID && 0x80000000)//extend id   
								{
									canfilter[channel_can_x][canfcnt[channel_can_x]].can_id = (CAN_EFF_MASK & can_logic[arraynum].dwEcuID) | CAN_EFF_FLAG;
									canfilter[channel_can_x][canfcnt[channel_can_x]].can_mask = CAN_EFF_MASK;
								}
								else
								{
									canfilter[channel_can_x][canfcnt[channel_can_x]].can_id = (CAN_SFF_MASK & can_logic[arraynum].dwEcuID);
									canfilter[channel_can_x][canfcnt[channel_can_x]].can_mask = CAN_SFF_MASK;
								}
								canid_diag_id[channel_can_x][canid_diag_cnt[channel_can_x]]=canfilter[channel_can_x][canfcnt[channel_can_x]].can_id;
								canid_diag_cnt[channel_can_x]++;								
								canfcnt[channel_can_x]++;		
#if dug_readcfg	> 0	
								printf_va_args("can_logic[%d]: ucLogicIndex:%d\n",arraynum,ucLogicIndex);
								printf_va_args("dwTesterID:0x%08x\n",can_logic[arraynum].dwTesterID);
								printf_va_args("dwEcuID:0x%08x\n",can_logic[arraynum].dwEcuID);
								printf_va_args("ucDiagType:0x%08x\n",can_logic[arraynum].ucDiagType);
								printf_va_args("ucLogicChanIndex:0x%08x\n",can_logic[arraynum].ucLogicChanIndex);
								printf_va_args("can_logic_num:%d\n",can_logic_num);
								printf_va_args("I2L: %d %d %d %d %d %d %d %d\n",I2L[0],I2L[1],I2L[2],I2L[3],I2L[4],I2L[5],I2L[6],I2L[7]);
#endif
							}														
						}						
						break;
						
					case DOWN_CONFIG_KLINE: 
						kline_channel = (typeheader.Status>>4) & 0x07;
						memcpy(&kline_config[kline_channel],(const void *)&data[0],24);
#if dug_readcfg	> 0					
						printf_va_args("kline_config[%d]:\n",kline_channel);
						printf_va_args("bIsValid:%d\n",kline_config[kline_channel].bIsValid);
						printf_va_args("ucDiagType:%d\n",kline_config[kline_channel].ucDiagType);
						printf_va_args("wBaudrate:%d\n",kline_config[kline_channel].wBaudrate);
						printf_va_args("Tester_InterByteTime_ms:%d\n",kline_config[kline_channel].Tester_InterByteTime_ms);
						printf_va_args("ECU_MaxInterByteTime_ms:%d\n",kline_config[kline_channel].ECU_MaxInterByteTime_ms);
						printf_va_args("ECU_MaxResponseTime_ms:%d\n",kline_config[kline_channel].ECU_MaxResponseTime_ms);
						printf_va_args("Tester_MaxNewRequestTime_ms:%d\n",kline_config[kline_channel].Tester_MaxNewRequestTime_ms);
						printf_va_args("nInitFrmLen:%d\n",kline_config[kline_channel].nInitFrmLen);
						printf_va_args("ucInitFrmData[10]:%d %d %d %d %d %d %d %d %d %d \n",
						kline_config[kline_channel].ucInitFrmData[0],
						kline_config[kline_channel].ucInitFrmData[1],
						kline_config[kline_channel].ucInitFrmData[2],
						kline_config[kline_channel].ucInitFrmData[3],
						kline_config[kline_channel].ucInitFrmData[4],
						kline_config[kline_channel].ucInitFrmData[5],
						kline_config[kline_channel].ucInitFrmData[6],
						kline_config[kline_channel].ucInitFrmData[7],
						kline_config[kline_channel].ucInitFrmData[8],
						kline_config[kline_channel].ucInitFrmData[9]);
#endif						
						break;
					case REQ_CONFIG_SIGNAL: 
						break;
					case DOWN_CONFIG_SIGNAL:
						cnt_cfg_signal++;
						switch(typeheader.Status)
						{
							case SIGNAL_DIAG_CAN:
								ucLogicIndex = data[7];  //8-->7								
								arraynum = CanIndex2Arraynum(ucLogicIndex);
								
								memcpy(&can_signal[signal_can_num],(const void *)&data[0],12);  //9-->12
#if dug_readcfg	> 0										
								printf_va_args("can_signal[%d]:\n",signal_can_num);								
								printf_va_args("dwLocalID:0x%08x\n",can_signal[signal_can_num].dwLocalID);				
								printf_va_args("ucRdDatSerID:0x%02x\n",can_signal[signal_can_num].ucRdDatSerID);
								printf_va_args("ucLidLength:%d\n",can_signal[signal_can_num].ucLidLength);
								printf_va_args("ucMemSize:%d\n",can_signal[signal_can_num].ucMemSize);
								printf_va_args("ucLogicChanIndex:%d\n",can_signal[signal_can_num].ucLogicChanIndex);
								printf_va_args("wSampleCyc:%d\n",can_signal[signal_can_num].wSampleCyc);
#endif
								can_signal[signal_can_num].dwNextCommTime = 0;
								can_logic[arraynum].pt_signal[can_logic[arraynum].ucSignalNum] = &can_signal[signal_can_num];
								signal_can_num++;
                            	can_logic[arraynum].ucSignalNum++;	
								
								if((ucLogicIndex>>4)==0)
								{
									g_edas_state.state_can0_15765 = 0;									
								}
								else if((ucLogicIndex>>4)==1)
								{
									g_edas_state.state_can1_15765 = 0;									
								}
								//My_Printf(dug_readcfg,"SIGNAL_DIAG_CAN:%d\n",signal_can_num);								
								break;
							case SIGNAL_DIAG_KLINE:
								ucLogicIndex = data[7];
								memcpy(&kline_siganl[siganl_kline_num],(const void *)&data[0],12); 
								//printf_va_args_en(dug_readcfg,"kline_siganl[%d]:\n",siganl_kline_num);
								
								//printf_va_args_en(dug_readcfg,"dwLocalID:0x%08x\n",kline_siganl[siganl_kline_num].dwLocalID);
								//printf_va_args_en(dug_readcfg,"ucRdDatSerID:0x%02x\n",kline_siganl[siganl_kline_num].ucRdDatSerID);
								//printf_va_args_en(dug_readcfg,"ucLidLength:%d\n",kline_siganl[siganl_kline_num].ucLidLength);
								//printf_va_args_en(dug_readcfg,"ucMemSize:%d\n",kline_siganl[siganl_kline_num].ucMemSize);
								//printf_va_args_en(dug_readcfg,"ucLogicChanIndex:0x%02x\n",kline_siganl[siganl_kline_num].ucLogicChanIndex);
								//printf_va_args_en(dug_readcfg,"wSampleCyc:%d\n",kline_siganl[siganl_kline_num].wSampleCyc);
                            	siganl_kline_num++;
								g_edas_state.state_k = 0;						
								break;
							case SIGNAL_EXTERNAL:
								break;
							case SIGNAL_SAE1939:
								memcpy(&CAN1939[CAN_1939_num].ID,(const void *)&data[0],4);
								
								CAN1939[CAN_1939_num].chan = data[6]>>4;								
								
								cchan = CAN1939[CAN_1939_num].chan;	

								//printf_va_args_en(dug_readcfg,"CAN1939[%d] id=0x%08x,%d\n",CAN_1939_num,CAN1939[CAN_1939_num].ID,CAN1939[CAN_1939_num].chan);
								
								
								if(CAN1939[CAN_1939_num].ID && 0x40000000)//extend id
								{
									canfilter[cchan][canfcnt[cchan]].can_id = (CAN_EFF_MASK & CAN1939[CAN_1939_num].ID) | CAN_EFF_FLAG;
									canfilter[cchan][canfcnt[cchan]].can_mask = CAN_EFF_MASK;
								}
								else
								{
									canfilter[cchan][canfcnt[cchan]].can_id  = (CAN_SFF_MASK & CAN1939[CAN_1939_num].ID);
									canfilter[cchan][canfcnt[cchan]].can_mask = CAN_SFF_MASK;
								}
								canfcnt[cchan]++;
								if(cchan == 0)
								{
									g_edas_state.state_can0_1939 = 0;
								}
								else
								{
									g_edas_state.state_can1_1939 = 0;
								}								
								CAN_1939_num++;
								break;
							case SIGNAL_GPS:
								isSaveGps = data[0];
								if(isSaveGps)
									g_edas_state.state_gps = 0;
								break;
							case SIGNAL_AUTH_CODE:
								memcpy(&auth_code[8],&data[0],300);								
								updateAuthCode();
								break;
						}
						break;
					case REQ_START_DAQ:
						break;
						
				}
				break;
			case STATUS_END:
				break;
		}
	}
	return cnt_cfg_signal;
}

void updateAuthCode(void)
{
	int i;
	unsigned int sum;
	unsigned char year,month,date,hour,minute,second;
	unsigned char rtctime[8];
	
	get_rtctime(rtctime);

	year   = rtctime[6];
	month  = 0x1F & rtctime[5];
	date   = 0x3F & rtctime[4];
	hour   = 0x3F & rtctime[2];
	minute = 0x7F & rtctime[1];
	second = 0x7F & rtctime[0];


	cData[0] = '2';
	cData[1] = '0';
	cData[2] = '0'+(year >> 4);
	cData[3] = '0'+(0x0F & year);
	cData[4] = '-';
	cData[5] = '0'+(month >> 4);
	cData[6] = '0'+(0x0F & month);
	cData[7] = '-';
	cData[8] = '0'+(date >> 4);
	cData[9] = '0'+(0x0F & date);

	cTime[0] = '0'+(hour >> 4);
	cTime[1] = '0'+(0x0F & hour);
	cTime[2] = '-';
	cTime[3] = '0'+(minute >> 4);
	cTime[4] = '0'+(0x0F & minute);
	cTime[5] = '-';
	cTime[6] = '0'+(second >> 4);
	cTime[7] = '0'+(0x0F & second);

	memcpy(&auth_code[212+8],cData,10);
	memcpy(&auth_code[222+8],cTime,8);
	auth_code[230+8] = ucTimestampType;	
	memcpy(&auth_code[231+8],cProgramName,8);
	memcpy(&auth_code[239+8],&DeviceID,4);
	


	auth_code[0] = 0x5A;
	auth_code[1] = 0x01;
	auth_code[2] = 0xF8;  //304 = 0x130;    504 =0x 1f8
	auth_code[3] = 0x01;

	auth_code[4] = UP_DAQ_SIGNAL;
	auth_code[5] = SIGNAL_AUTH_CODE;
	auth_code[6] = 0xF4;  //300 = 0x12C   500=0x1f4
	auth_code[7] = 0x01;


	sum = 0;
	for(i=0;i<508;i++)
	{
		sum += auth_code[i];
	}
	auth_code[508] = (0xFF & sum);

}

void read_user_cfgset(void)
{
	if(ReadCfg()>0)
	{
#if dug_readcfg > 0
		printf_va_args("readcfg done!\n");
#endif
	}
	else
	{
		edas_led_erron();
#if dug_readcfg > 0
		printf_va_args("readcfg failed!\n");
#endif
	}
}


