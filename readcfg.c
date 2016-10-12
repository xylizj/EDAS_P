#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <net/if.h>
#include <linux/socket.h>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>

#include "rtc.h"
#include "recfile.h"
#include "can.h"
#include "gpio.h"
#include "net3g.h"
#include "readcfg.h"


struct can_filter canfilter[CAN_CHANNEL_MAX][100];
int canfcnt[2];

unsigned int  canid_diag_id[2][8];
unsigned int  canid_diag_cnt[2];



tCAN_COM_CONFIG can_channel[CAN_CHANNEL_MAX];
tCAN_LOGIC_CONFIG can_logic[CAN_LOGIC_MAX];/*0~3:for CAN0;4-7:for CAN1*/
tCAN_SIGNAL can_signal[CAN_SIGNAL_MAX];
uint8_t can_logic_num = 0;



static void parse_basic_cfg(FILE *fp_cfg)
{
	#define BUFF_LEN 100
	char buff[BUFF_LEN];
	int state;
	
	state = 0;	
	while(fgets(buff,BUFF_LEN,fp_cfg)!=NULL)
	{
		switch(state)
		{
			case 0:
				if((buff[1]=='I')&&(buff[2]=='P'))	 //IP_SET
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
				net3g.ServerIP = (char *)malloc(20);
				if(NULL != net3g.ServerIP)
					memcpy(net3g.ServerIP,buff,strlen(buff));
				memset(buff,0,BUFF_LEN);
				state = 0;
				break;
			case 2:
				net3g.ServerPort = (char *)malloc(10);
				if(NULL != net3g.ServerPort)
					memcpy(net3g.ServerPort,buff,strlen(buff));
				memset(buff,0,BUFF_LEN);
				state = 0;
				break;
			case 3:
				g_sys_info.CAR_ID = (char *)malloc(strlen(buff));
				if(NULL == g_sys_info.CAR_ID)
				{
					printf("CARD_ID memory allocation failed!\n");
				}
				else
				{
					memcpy(g_sys_info.CAR_ID,buff,strlen(buff));					
					printf("CARD_ID:%s\n",g_sys_info.CAR_ID);
				}

				memset(buff,0,BUFF_LEN);
				state = 0;
				break;
			case 4:
				g_sys_info.DeviceID = atoi(buff);
				memset(buff,0,BUFF_LEN);
				state = 0;
				break;					
		}
	}
	fclose(fp_cfg);
}



void read_edas_cfg(void)
{
	FILE *fp_cfg;

	fp_cfg = fopen("/media/sd-mmcblk0p1/EDASCFG","r");
	if(fp_cfg != NULL)
		parse_basic_cfg(fp_cfg);
}

char CanIndex2Arraynum(char index)
{
    char hi,lo;
    hi = (index & 0xF0)>>2;
    lo = index & 0x0F;
    return (lo+hi);
}

static
void acquire_datetime(char cDate[10], char cTime[8])
{
	unsigned char rtctime[8];
	unsigned char year,month,date,hour,minute,second;

	get_rtctime(rtctime);

	year   = rtctime[6];
	month  = 0x1F & rtctime[5];
	date   = 0x3F & rtctime[4];
	hour   = 0x3F & rtctime[2];
	minute = 0x7F & rtctime[1];
	second = 0x7F & rtctime[0];

	cDate[0] = '2';
	cDate[1] = '0';
	cDate[2] = '0'+(year >> 4);
	cDate[3] = '0'+(0x0F & year);
	cDate[4] = '-';
	cDate[5] = '0'+(month >> 4);
	cDate[6] = '0'+(0x0F & month);
	cDate[7] = '-';
	cDate[8] = '0'+(date >> 4);
	cDate[9] = '0'+(0x0F & date);

	cTime[0] = '0'+(hour >> 4);
	cTime[1] = '0'+(0x0F & hour);
	cTime[2] = '-';
	cTime[3] = '0'+(minute >> 4);
	cTime[4] = '0'+(0x0F & minute);
	cTime[5] = '-';
	cTime[6] = '0'+(second >> 4);
	cTime[7] = '0'+(0x0F & second);
}


void updateAuthCode(void)
{
	char cDate[10];
	char cTime[8];
	char cProgramName[8] = "EDAS_P";
	int i;
	unsigned int sum;
	
	acquire_datetime(cDate, cTime);

	memcpy(&auth_code[212+8],cDate,10);
	memcpy(&auth_code[222+8],cTime,8);
	auth_code[230+8] = 2;//ucTimestampType;	
	memcpy(&auth_code[231+8],cProgramName,8);
	memcpy(&auth_code[239+8],(void *)&g_sys_info.DeviceID,4);
	
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


static 
void init_com(void)
{
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
#if DEBUG_CFG	> 0						
	printf_va_args("REQ_CONFIG_COM\n");
#endif
}


static inline
void print_can_cfgset(unsigned char channel_can_x, unsigned char ucLogicIndex, unsigned char arraynum)
{
	printf_va_args("can_channel[]: %d \n",channel_can_x);
	printf_va_args("bIsValid:%d\n",can_channel[channel_can_x].bIsValid);
	printf_va_args("nLogicDiagChanNum:%d\n",can_channel[channel_can_x].nLogicDiagChanNum);
	printf_va_args("wBaudrate:%d\n",can_channel[channel_can_x].wBaudrate);
						
	printf_va_args("can_logic[%d]: ucLogicIndex:%d\n",arraynum,ucLogicIndex);
	printf_va_args("dwTesterID:0x%08x\n",can_logic[arraynum].dwTesterID);
	printf_va_args("dwEcuID:0x%08x\n",can_logic[arraynum].dwEcuID);
	printf_va_args("ucDiagType:0x%08x\n",can_logic[arraynum].ucDiagType);
	printf_va_args("ucLogicChanIndex:0x%08x\n",can_logic[arraynum].ucLogicChanIndex);
	printf_va_args("can_logic_num:%d\n",can_logic_num);
	printf_va_args("I2L: %d %d %d %d %d %d %d %d\n",I2L[0],I2L[1],I2L[2],I2L[3],I2L[4],I2L[5],I2L[6],I2L[7]);
}

static inline
void print_k_cfgset(uint8_t kline_channel)
{
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
}

static inline
void print_diag_cfgset(uint8_t signal_can_num)
{
	printf_va_args("can_signal[%d]:\n",signal_can_num); 							
	printf_va_args("dwLocalID:0x%08x\n",can_signal[signal_can_num].dwLocalID);				
	printf_va_args("ucRdDatSerID:0x%02x\n",can_signal[signal_can_num].ucRdDatSerID);
	printf_va_args("ucLidLength:%d\n",can_signal[signal_can_num].ucLidLength);
	printf_va_args("ucMemSize:%d\n",can_signal[signal_can_num].ucMemSize);
	printf_va_args("ucLogicChanIndex:%d\n",can_signal[signal_can_num].ucLogicChanIndex);
	printf_va_args("wSampleCyc:%d\n",can_signal[signal_can_num].wSampleCyc);
}


static
void fill_canfilter_1939(char *data)
{
	unsigned char cchan;
	unsigned int id;
	
	cchan = data[6]>>4;
	id = *((unsigned int*)&data[0]);
	if(id & 0x40000000)//if(CAN1939[CAN_1939_num].ID && 0x40000000)//extend id
	{
		canfilter[cchan][canfcnt[cchan]].can_id = (CAN_EFF_MASK & id) | CAN_EFF_FLAG;
		canfilter[cchan][canfcnt[cchan]].can_mask = CAN_EFF_MASK;
	}
	else
	{
		canfilter[cchan][canfcnt[cchan]].can_id  = (CAN_SFF_MASK & id);
		canfilter[cchan][canfcnt[cchan]].can_mask = CAN_SFF_MASK;
	}
	canfcnt[cchan]++;
	if(cchan == 0)
	{
		g_sys_info.state_can0_1939 = 0;
	}
	else
	{
		g_sys_info.state_can1_1939 = 0;
	}																
}

static 
int read_cfgset(void)
{
	FILE *fp;
	unsigned int iCount;
	unsigned int i,j,ch;
	READ_STATUS state;
	tHEADER header;
	tTYPE_HEADER typeheader;
	unsigned int head_rcv_cnt;
	unsigned int data_rcv_cnt;
	unsigned char ucLogicIndex,arraynum;
	
	int tem_id;
	int cnt_cfg_signal = 0;

	unsigned char channel_can_x;
	uint8_t kline_channel;
	uint8_t signal_can_num = 0;

	char *data = (char *)malloc(500);//char data[500];
	if(NULL == data)
	{
		printf_va_args("read_cfgset memory allocation failed!\n");
		return -1;
	}
	
	fp = fopen(EDASCFGFILE,"rb");
	if(fp == NULL)
	{
		return -1;
	}
	
	fseek(fp,0,SEEK_END);
	iCount = ftell(fp);
	rewind(fp);
	state = STATUS_IDLE;
	data_rcv_cnt = 0;
	
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
					header.Fmt = ch;
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
						}
						else
						{
							state = CHKSUM_RECEIVING;
						}
						break;
					default:
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
						break;
					case REQ_CONFIG_COM://do nothing
						init_com();
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
#if DEBUG_CFG	> 0	
								print_can_cfgset(channel_can_x,ucLogicIndex,arraynum);
#endif
							}														
						}						
						break;
						
					case DOWN_CONFIG_KLINE: 
						kline_channel = (typeheader.Status>>4) & 0x07;
						memcpy(&kline_config[kline_channel],(const void *)&data[0],24);
#if DEBUG_CFG	> 0					
						print_k_cfgset(kline_channel);
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
#if DEBUG_CFG	> 0				
								print_diag_cfgset(signal_can_num);
#endif
								can_signal[signal_can_num].dwNextCommTime = 0;
								can_logic[arraynum].pt_signal[can_logic[arraynum].ucSignalNum] = &can_signal[signal_can_num];
								signal_can_num++;
                            	can_logic[arraynum].ucSignalNum++;	
								
								if((ucLogicIndex>>4)==0)
								{
									g_sys_info.state_can0_15765 = 0;									
								}
								else if((ucLogicIndex>>4)==1)
								{
									g_sys_info.state_can1_15765 = 0;									
								}									
								break;
							case SIGNAL_DIAG_KLINE:
								ucLogicIndex = data[7];

								kline_siganl[k_sig_num] = (tKLINE_SIGNAL *)calloc(1,sizeof(tKLINE_SIGNAL));
								if(NULL != kline_siganl[k_sig_num]){
									memcpy(kline_siganl[k_sig_num],(const void *)&data[0],12); 
				                	k_sig_num++;
									g_sys_info.state_k = 0;		
								}
								break;
							case SIGNAL_EXTERNAL:
								break;
							case SIGNAL_SAE1939:
								fill_canfilter_1939(data);
							break;
							case SIGNAL_GPS:
								g_sys_info.savegps = data[0];
								if(g_sys_info.savegps)
									g_sys_info.state_gps = 0;
								break;
							case SIGNAL_AUTH_CODE:
								auth_code = (unsigned char *)malloc(509);
								if(NULL != auth_code){
										memcpy(&auth_code[8],&data[0],300);								
										updateAuthCode();
									}
								break;
						}
						break;
					case REQ_START_DAQ:
						break;
						
				}
				break;
			case STATUS_END:
				break;
			default:
				break;
		}
	}

	free(data);
	data = NULL;

	return cnt_cfg_signal;
}


void acquire_user_cfgset(void)
{
	if(read_cfgset()>0)
	{
#if DEBUG_CFG > 0
		printf_va_args("readcfg done!\n");
#endif
	}
	else
	{
		edas_led_erron();
#if DEBUG_CFG > 0
		printf_va_args("readcfg failed!\n");
#endif
	}
}


