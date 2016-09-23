#include <fcntl.h>

#include "task.h"
#include "sd.h"
#include "gps.h"
#include "gpio.h"
#include "common.h"
#include "readcfg.h"
#include "kline.h"

unsigned char record_filename[255];

void task_sd(void)
{
	FILE *fp;	
	int isCurRcdFileOpen;
	int iCount;

	pthread_mutex_lock(&g_file_mutex); 
	getFilesRecords();
	getFilesName();
	writeFilesRecords();
	pthread_mutex_unlock(&g_file_mutex); 
	
	while(1)
	{
		sleep(10);
		
		fileTransState = IDLE;
#if dug_readcfg > 0
		printf_va_args("g_sys_info.state_T15:%d\n",g_sys_info.state_T15);
#endif
		while(g_sys_info.state_T15 == 0)
		{
			sleep(1);
		}
#if dug_readcfg > 0
		printf_va_args("g_sys_info.state_T15:%d\n",g_sys_info.state_T15);
#endif 
		getSDstatus(&sd_total,&sd_used,&sd_free);
		db_sd_free = sd_free/1024; 
		creat_FileName(RECORDPATH,record_filename); 
#if dug_readcfg > 0
				printf_va_args("record_filename is %s\n",record_filename);
#endif 
		fp=fopen(record_filename,"w+b");	
		if(fp > 0)
		{
#if dug_readcfg > 0
			printf_va_args("create file ok,file name=%s\n",record_filename);
#endif 			 
		} 
		isCurRcdFileOpen = 1;
		
		fwrite(auth_code,509,1,fp);			
		SDBufferInit();
	
		while(g_sys_info.state_T15)
		{		
			while(g_isDownloadCfg)
			{
				if(isCurRcdFileOpen == 1)
				{
					fclose(fp);
					isCurRcdFileOpen = 0;
				}
				sleep(1);
			}
			if(can1939buf[0].cnt)
			{
				Write2SDBuffer(fp,&can1939buf[0].qdata[can1939buf[0].rp].data[0], can1939buf[0].qdata[can1939buf[0].rp].len);
	            can1939buf[0].rp++;
	            if(can1939buf[0].rp == CAN_RCV1939_QUEUE_SIZE)  can1939buf[0].rp = 0;    
	            can1939buf[0].cnt--;
			}
			if(can1939buf[1].cnt)
			{
				Write2SDBuffer(fp,&can1939buf[1].qdata[can1939buf[1].rp].data[0], can1939buf[1].qdata[can1939buf[1].rp].len);
	            can1939buf[1].rp++;
	            if(can1939buf[1].rp == CAN_RCV1939_QUEUE_SIZE)  can1939buf[1].rp = 0;    
	            can1939buf[1].cnt--;
			}
			if(can15765buf.cnt)
			{
#if dug_can > 0
				printf_va_args("can15765buf.cnt:%d,len:%d\n",can15765buf.cnt,can15765buf.qdata[can15765buf.rp].len);
#endif
				Write2SDBuffer(fp,&can15765buf.qdata[can15765buf.rp].data[0], can15765buf.qdata[can15765buf.rp].len);
				can15765buf.rp++;
	            if(can15765buf.rp == CAN_RCV15765_QUEUE_SIZE)  can15765buf.rp = 0;    
	            can15765buf.cnt--;
			}
			if(k15765buf.cnt)
			{
				Write2SDBuffer(fp,&k15765buf.qdata[k15765buf.rp].data[0], k15765buf.qdata[k15765buf.rp].len);
				k15765buf.rp++;
	            if(k15765buf.rp == K_RCV15765_QUEUE_SIZE)  k15765buf.rp = 0;    
	            k15765buf.cnt--;
			}
			if(bps_buf.cnt)
			{
				Write2SDBuffer(fp,&bps_buf.qdata[bps_buf.rp].data[0], bps_buf.qdata[bps_buf.rp].len);
				bps_buf.rp++;
	            if(bps_buf.rp == GPS_RCV_QUEUE_SIZE)  bps_buf.rp = 0;    
	            bps_buf.cnt--;
			}
			usleep(2000);
		}
		if(isCurRcdFileOpen == 1)
		{
			Write2SDBufferDone(fp);
			isCurRcdFileOpen = 0;
		}
		//pthread_mutex_lock(&g_file_mutex); 
		memcpy(filesRecord[filescnt].filename,&record_filename[27],strlen(record_filename)-27);
		filesRecord[filescnt].flag = 0;
		filescnt++;		
		//pthread_mutex_unlock(&g_file_mutex); 
	}
}


void task_gps(void)
{
	int len;
	int fd;	

	char *gps_ser_buf = (char *)malloc(GPS_SERIAL_BUF_LEN);
	if(NULL == gps_ser_buf)
		return;

	fd = openSerial("/dev/ttySP1");
	
	while (1) 
	{
		len = read(fd, gps_ser_buf, GPS_SERIAL_BUF_LEN);	
		usleep(1000);
	
		if(TRUE == process_gps_ser_buf(gps_ser_buf, len))
		{			
			gps_info_parse();	
		}
	}
}


void task_wd()
{
	while(1)
	{
		edas_wd_on();
		usleep(50000);
		edas_wd_off();
		usleep(50000);
	}
}


int ulFlashRxId[3],ulFlashTxId[3];
volatile uint32_t currentlid;
volatile uint32_t currentMSize;

int currenLogicChan;
uint8_t currentLidlen;
uint8_t b15765rcv;
uint8_t I2L[CAN_LOGIC_MAX]; /*indexnum to logic number*/   /*查找I对应具体的逻辑通道值*/


static void task_canTP(void)
{
	int i;
	while(1)
	{
		if(g_sys_info.state_T15 == 0)
		{
			sleep(1);
			continue;
		}
		for(i=0;i<can_logic_num;i++)
		{			
            switch(TpTxState[I2L[i]])
            {
                case TP_TX_REQUEST:
                if( outq[I2L[i]].cnt == 1 )
                {
                    if(DirectCanTransmit(I2L[i]) == 0)
                    {      
                    	Can_TxConfirmation(I2L[i],CAN_ERR_TX_OK);
                        TpTxState[I2L[i]] = TP_TX_IDLE;
                        ucTxTimer[I2L[i]] = 0;
                    }
                }
                else if( outq[I2L[i]].cnt > 1)
                {
                    if(0 == DirectCanTransmit(I2L[i]))
                    {
                        TpTxState[I2L[i]] = TP_TX_WAIT_FC;
                        ucTxTimer[I2L[i]] = kTxTimeout;
                        fc_count[I2L[i]] = 50;
                    }
                }
                break;

                //  2012-08-29, lzy, We need this in case heavy bus load

                case TP_TX_FC:      //2009-11-10, lzy, add for tx flow control frame   
                if( outq[I2L[i]].cnt >= 1 )
                {
                    if(0 == DirectCanTransmit(I2L[i]))
                    {
                        TpRxState[I2L[i]] = TP_RX_CF;
                        uiRxTimer[I2L[i]] = kRxTimeout;
                    }
                }
                break;

                case TP_TX_WAIT_FC:
                //fc_count++;
                if( fc_count[I2L[i]] )
                {
                    fc_count[I2L[i]]--;
                    if( !fc_count[I2L[i]] )
                    {
                        TpTxState[I2L[i]] = TP_TX_IDLE; 
						Can_ErrIndication(I2L[i],CAN_ERR_RX_TIMEOUT);
                        ucTxTimer[I2L[i]] = 0x00;
                    }
                }
                break;

                case TP_TX_CF:
                //2013-06-08, fixed error in case ucTxMin = 0;
                if( ucTxTimer[I2L[i]] )
                {
                    ucTxTimer[I2L[i]]--;
                }

                if( !ucTxTimer[I2L[i]] )
                {
                    if( outq[I2L[i]].cnt > 0 )
                    {
                        if(0 == DirectCanTransmit(I2L[i]))
                        {
                            if( outq[I2L[i]].cnt == 0 )
                            {       
                            	Can_TxConfirmation(I2L[i],CAN_ERR_TX_OK);
                                TpTxState[I2L[i]] = TP_TX_IDLE;
                                ucTxTimer[I2L[i]] = 0;
                            }
                            else
                            {
                                TpTxState[I2L[i]] = TP_TX_CF;
                                ucTxTimer[I2L[i]] = ucTSmin[I2L[i]];
                            }
                        }
                        else
                        {
                            ucTxTimer[I2L[i]]++;
                        }
                    }
                }
                break;

                default:
                break;
            }
        }
		usleep(1000);
	}
}

void create_can0_task(void)
{		
	int rate_cnt;
	int fd;
	char rate[8][10]={"125000","250000","","500000","","","","1000000"};
	struct ifreq ifr0;
	int ret;
	struct sockaddr_can addr0;
	pthread_t id_can0_read;
		
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
#if dug_can > 0 		
	printf_va_args("ifconfig can0 up,rate_cnt:%d,bitrate:%s!\n",rate_cnt,rate[rate_cnt]);
#endif
	s_can0 = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	strcpy(ifr0.ifr_name, "can0");
	ret = ioctl(s_can0, SIOCGIFINDEX, &ifr0);
	addr0.can_family = PF_CAN;
	addr0.can_ifindex = ifr0.ifr_ifindex;
	ret = bind(s_can0, (struct sockaddr *)&addr0, sizeof(addr0));
	if (ret < 0) {
		perror("bind can0 failed");
	}
	ret = setsockopt(s_can0, SOL_CAN_RAW, CAN_RAW_FILTER, &canfilter[0], canfcnt[0] * sizeof(canfilter[0][0]));
#if dug_can > 0 		
	if (ret < 0) {
				printf_va_args("setsockopt can0 failed");
	}
#endif
	ret=pthread_create(&id_can0_read,NULL,(void *)task_can0_read,NULL);
#if dug_can > 0 		
	if(ret!=0)
		printf_va_args("prhtead can0read created");
#endif
}



void create_can1_task(void)
{
	int rate_cnt;
	int fd;
	char rate[8][10]={"125000","250000","","500000","","","","1000000"};
	struct ifreq ifr1;
	int ret;
	struct sockaddr_can addr1;
	pthread_t id_can1_read;

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
#if dug_can > 0		
	printf_va_args("ifconfig can1 up,rate_cnt:%d,bitrate:%s!\n",rate_cnt,rate[rate_cnt]);
#endif
	s_can1 = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	strcpy(ifr1.ifr_name, "can1");
	ret = ioctl(s_can1, SIOCGIFINDEX, &ifr1);
	addr1.can_family = PF_CAN;
	addr1.can_ifindex = ifr1.ifr_ifindex;
	bind(s_can1, (struct sockaddr *)&addr1, sizeof(addr1));
	ret = setsockopt(s_can1, SOL_CAN_RAW, CAN_RAW_FILTER, &canfilter[1], canfcnt[1] * sizeof(canfilter[1][0]));

	ret=pthread_create(&id_can1_read,NULL,(void *)task_can1_read,NULL);
#if dug_can > 0		
	if(ret!=0)
		printf_va_args("prhtead can1read cread1");
#endif
}


void can_info_process(void)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int m = 0;
	uint8_t buffer[10];

	for(i=0;i<CAN_CHANNEL_MAX;i++)
	{			
		for(j=0;j< can_channel[i].ucLogicChanNum;j++)
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
								default: break;
							}
							Can_SendIso15765Buff(buffer,can_channel[i].pt_logic_can[j]->pt_signal[k]->ucLidLength+1,(i<<2+j));	
#if dug_can > 0
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
							Can_SendIso15765Buff(buffer,can_channel[i].pt_logic_can[j]->pt_signal[k]->ucMemSize+2,(i<<2+j));
#if dug_can
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


void task_can(void)
{
	pthread_t id_canTP;

	USB_Flash_Status[0]=KWP_IDLE;
	USB_Flash_Status[1]=KWP_IDLE;
		
	if(can_channel[0].bIsValid)
		create_can0_task();

	if(can_channel[1].bIsValid)
		create_can1_task();

	pthread_create(&id_canTP,NULL,(void *)task_canTP,NULL);
	
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
		can_info_process();		
		usleep(5000);
	}		
}







char cur3gfilename[100];
int cur3gfilename_len;


void HeartBeatReport(int sockfd,const struct sockaddr_in *addr,int len)
{
	static int kline_break_cnt = 0;
	
	static int can0_1939_break_cnt = 0;
	static int can1_1939_break_cnt = 0;
	static int can0_15765_break_cnt = 0;
	static int can1_15765_break_cnt = 0;
	
	char buf[MAX_SIZE]={0};
	char Type = 0x6C;
	char Reserve = 0x00;
	//short DataLen = 8*6;
	short DataLen = 8*7; //add edas_state
	char time[8];
	int cnt;
	unsigned short gpsDataLen;
	
	buf[0] = Type;
	buf[1] = Reserve;
	memcpy(&buf[2],&DeviceID,4);
	memcpy(&buf[6],&DataLen,2);

	memcpy(&buf[8],&time,8);
	memcpy(&buf[16],(void *)(&longitude),8);
	memcpy(&buf[24],(void *)(&latitude),8);
	memcpy(&buf[32],(void *)(&height),8);
	memcpy(&buf[40],(void *)(&speed),8);
	
	memcpy(&buf[48],(void *)(&db_sd_free),8);
	memcpy(&buf[56],(void *)(&g_sys_info),8);
				

	#if dug_3g > 0
	printf_va_args("3g_latitude  = %f\n",longitude);
	printf_va_args("3g_longitude = %f\n",latitude);
	printf_va_args("3g_speed	  = %f\n",height);
	printf_va_args("3g_height	  = %f\n",speed);
	printf_va_args("3g_sd_free  = %d\n",db_sd_free);
	#endif

	chksum(buf,DataLen+8);
	cnt = sendto(sockfd,buf,DataLen+9,0,(struct sockaddr *)addr,len);
#if dug_3g > 0
	printf_va_args("snd heartbeak,cnt:%d\n",cnt);
#endif
	if(g_sys_info.state_gps != 2)
		g_sys_info.state_gps = 0;
	if(g_sys_info.state_k != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_k == 0)
			{
				kline_break_cnt++;
				if(kline_break_cnt > 100)
				{
#if dug_3g > 0
					printf_va_args("kline_break_reboot!\n");
#endif
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				kline_break_cnt = 0;
			}
		}
		g_sys_info.state_k = 0;
		
	}
	if(g_sys_info.state_can0_1939 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can0_1939 == 0)
			{
				can0_1939_break_cnt++;
				if(can0_1939_break_cnt > 100)
				{
#if dug_sys > 0
					printf_va_args("can0_1939_break_reboot!\n");
#endif
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can0_1939_break_cnt = 0;
			}
		}
		g_sys_info.state_can0_1939  = 0;
	}
	if(g_sys_info.state_can0_15765 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can0_15765 == 0)
			{
				can0_15765_break_cnt++;
				if(can0_15765_break_cnt > 100)
				{
#if dug_sys > 0
					printf_va_args("can0_15765_break_reboot!\n");
#endif					
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can0_15765_break_cnt = 0;
			}
		}
		g_sys_info.state_can0_15765 = 0;
	}
	if(g_sys_info.state_can1_1939 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can1_1939 == 0)
			{
				can1_1939_break_cnt++;
				if(can1_1939_break_cnt > 100)
				{
#if dug_sys > 0
					printf_va_args("can1_1939_break_reboot!\n");
#endif 
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can1_1939_break_cnt = 0;
			}
		}
		g_sys_info.state_can1_1939  = 0;
	}
	if(g_sys_info.state_can1_15765 != 2)
	{
		if(g_sys_info.state_T15==1)
		{
			if(g_sys_info.state_can1_15765 == 0)
			{
				can1_15765_break_cnt++;
				if(can1_15765_break_cnt > 100)
				{
#if dug_sys > 0
					printf_va_args("can1_15765_break_reboot!\n");
#endif 
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can1_15765_break_cnt = 0;
			}
		}
		g_sys_info.state_can1_15765 = 0;
	}
	if((g_sys_info.state_T15==1)&&(isSaveGps == 1))
	{
		//save heartbeat data
	}
/////////////////////////////////
		gpsDataLen = 32+8;
		DataLen = gpsDataLen+12;
		bps_buf.qdata[bps_buf.wp].data[0] = UP_DAQ_SIGNAL;
        bps_buf.qdata[bps_buf.wp].data[1] = SIGNAL_GPS;
        bps_buf.qdata[bps_buf.wp].data[2] = 0xFF & DataLen;
        bps_buf.qdata[bps_buf.wp].data[3] = DataLen>>8;

		bps_buf.qdata[bps_buf.wp].data[4] = 0x01;
        bps_buf.qdata[bps_buf.wp].data[5] = 0x00;
        bps_buf.qdata[bps_buf.wp].data[6] = 0x00;
        bps_buf.qdata[bps_buf.wp].data[7] = 0x00;

	   	memcpy(&bps_buf.qdata[bps_buf.wp].data[8],(void*)&US_SECOND,4);
		memcpy(&bps_buf.qdata[bps_buf.wp].data[12],(void*)&US_MILISECOND,2);
		
		memcpy(&bps_buf.qdata[bps_buf.wp].data[14],&gpsDataLen,2);
		
		memcpy(&bps_buf.qdata[bps_buf.wp].data[16],(void*)&longitude,8);
		memcpy(&bps_buf.qdata[bps_buf.wp].data[24],(void*)&latitude,8);
		memcpy(&bps_buf.qdata[bps_buf.wp].data[32],(void*)&height,8);
		memcpy(&bps_buf.qdata[bps_buf.wp].data[40],(void*)&speed,8);	        

        bps_buf.qdata[bps_buf.wp].len = DataLen+4;
        bps_buf.wp++;
        if(bps_buf.wp == GPS_RCV_QUEUE_SIZE)  bps_buf.wp = 0;    
        bps_buf.cnt++;
////////////////////////////////	
}


