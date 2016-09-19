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
#include "boot.h"
#include "can.h"
#include "readcfg.h"
#include "kline.h"
#include "edas_gpio.h"
#include "net3g.h"
#include "edas_rtc.h"
#include "uploadingfile.h"
#include <string.h>
#include "common.h"
#include "LinkQueue.h"

int T15_state = 1;
int File_UPLOADING = 0;  
int File_CUR_READ = 0;  

//int DeviceID= 0x00000003;
unsigned int DeviceID= 20150801; // for DBTS
//unsigned int DeviceID= 20151112;  //for CVS


volatile int g_pppd_pid = 0;

char CAR_ID[40] = "E0801"; //for DBTS


tSD_buffer SD_buffer;

unsigned char record_filename[255];
FILE *fp_curfile_t;

LinkQueue msg_queue;

pthread_mutex_t g_file_mutex;

pthread_mutex_t g_rtc_mutex;


unsigned int g_SysCnt;
unsigned int g_Rcv3gCnt = 0;
unsigned int g_LastRcvCnt = 0;
unsigned int g_3gState = 0;
unsigned int g_TimeoutCnt = 0;
volatile unsigned int g_3gTryCnt = 0;


char g_ServerIP[20];
char g_ServerPort[10];

volatile unsigned int sd_total;
volatile unsigned int sd_used;
volatile unsigned int sd_free;
volatile double db_sd_free; 

volatile int g_isDownloadCfg = 0;
volatile int g_FileSizeofCfg;
char g_cfgFile[5120];
int g_cfgFilecnt[10] = {0};
volatile int g_cfgFileblock = 0;
FILE *g_fp_cfg; 
volatile int g_fp_cfg_state = 0;

void main(int argc,char *argv[])
{
	int i;
	int ppp_pid;
	char killcmd[50];
	//int fp_popen;
	FILE *fp_cfg;
	char buff[100];
	int state = 0;

	pthread_t id_can,id_k,id_sd,id_k_1ms,id_3g,id_gps,id_CheSndFile,id_wd;
	pthread_t id_udprcv,id_handle_msg,id_checkmsgrx;
	pthread_t id_heartbeat;
	int ret,res;

	UpdateEdas();
	init_EDAS_State();
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

#if	CFG_GPS_ENABLE
	ret=pthread_create(&id_gps,NULL,(void *)task_gps,NULL);
#endif
	
	getSDstatus(&sd_total,&sd_used,&sd_free);
	db_sd_free = (double)(sd_free/1024);
	
	InitQueue(&msg_queue);
	init_gpio();
	edas_power_on();
	init_adc();	
	
#if	CFG_3G_ENABLE	
	start3G();
#endif
	ret=pthread_create(&id_wd,NULL,(void *)task_wd,NULL);
	
	res = pthread_mutex_init(&g_file_mutex, NULL);
	res = pthread_mutex_init(&g_rtc_mutex, NULL);  
	
    if(res != 0)  
    {  
        perror("pthread_mutex_init failed\n");  
        exit(EXIT_FAILURE);  
    }
	//My_Printf(dug_readcfg,"newname is %s\n",record_filename);
	if(ReadCfg()>0)
	{
		g_edas_state.state_cfg = 1;
	}
	else
	{
		edas_led_erron();
		g_edas_state.state_cfg = 0;
	}
	My_Printf(dug_readcfg,"readcfg done!\n");
	curMsgstate.state = MsgState_rcv;	
	
#if	CFG_3G_ENABLE
	init_3g_net();
#endif
	g_3gState = 1;
	ret=pthread_create(&id_can,       NULL,(void *)task_can,        NULL);
	ret=pthread_create(&id_k,         NULL,(void *)task_k,          NULL);
	ret=pthread_create(&id_sd,        NULL,(void *)task_sd,         NULL);
	
#if	CFG_3G_ENABLE
	ret=pthread_create(&id_heartbeat, NULL,(void *)task_heartbeat,  NULL);
	ret=pthread_create(&id_CheSndFile,NULL,(void *)task_ChkSndFile, NULL);
	ret=pthread_create(&id_udprcv,    NULL,(void *)task_udprcv,     NULL);
	ret=pthread_create(&id_handle_msg,NULL,(void *)task_handle_msg, NULL);
	ret=pthread_create(&id_checkmsgrx,NULL,(void *)task_check_msgrx,NULL);
#endif	

	while(1)
	{
		T15_state = is_T15_on();
		g_edas_state.state_T15 = T15_state;
		
		usleep(500000);


		g_SysCnt++;
		if(curnetstat==1)
		{
			led_usb_blink();
		}
		else
		{
			edas_led_usbon();
		}
		edas_led_rxoff();
		edas_led_txoff();
		edas_led_koff();
		edas_led_erroff();
		if((g_SysCnt % 120) == 0) 
		{
#if	CFG_3G_ENABLE
			if(g_Rcv3gCnt > g_LastRcvCnt)
			{
				g_LastRcvCnt = g_Rcv3gCnt;
				g_TimeoutCnt = 0;
				g_3gTryCnt = 0;
			}
			else
#endif
			{
#if	CFG_3G_ENABLE
				g_TimeoutCnt++;

				if(g_TimeoutCnt < 2)
				{
					continue;
				}
				
				curnetstat = 0;
				ppp_pid = g_pppd_pid;
				if(ppp_pid > 0)  //kill pppd
				{
					g_3gState = 0;									
					makeKillCommand(killcmd,ppp_pid);
					system(killcmd);
					sleep(1);
					waitpid(ppp_pid,NULL,0);				
					sleep(5);					
					start3G();
					reinit_3g_net();
					g_3gState = 1;
					g_TimeoutCnt = 0;
				}
#endif
				if(T15_state == 0)  //if t15 down and 3g cannot reach, reboot system!
				{
					g_3gTryCnt++;
					if(g_3gTryCnt > 20)
					{
						My_Printf(dug_sys,"t15 down and 3g cannot reach_reboot!\n");
						sleep(1);
						system("reboot");
					}
				}
				
			}
		}
	}
	DestroyQueue(&msg_queue);
}

int ulFlashRxId[3],ulFlashTxId[3];
volatile uint32_t currentlid;
volatile uint32_t currentMSize;

int currenLogicChan;
uint8_t currentLidlen;
uint8_t b15765rcv;
uint8_t I2L[CAN_LOGIC_MAX]; /*indexnum to logic number*/   /*查找I对应具体的逻辑通道值*/


void task_canTP()
{
	int i;
	while(1)
	{
		if(T15_state == 0)
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

int test_cnt = 0;
void task_can()
{
	int ret;
	int i = 0;
	int j = 0;
	int k = 0;
	int rate_cnt;
	int m = 0;
	pthread_t id_canTP;

	uint8_t buffer[10];
	int fd;
	struct sockaddr_can addr0,addr1;
	struct ifreq ifr0,ifr1;

	struct can_frame fr, frdup;

	fd_set rset;
	
	pthread_t id_can0_read;
	char rate[8][10]={"125000","250000","","500000","","","","1000000"};

	USB_Flash_Status[0]=KWP_IDLE;
    USB_Flash_Status[1]=KWP_IDLE;
		
	if(can_channel[0].bIsValid)
	{		
		system("ifconfig can0 down");
		sleep(1);
		rate_cnt = can_channel[0].wBaudrate/125 - 1;		

		if((fd=(open("/sys/devices/platform/FlexCAN.0/bitrate",O_CREAT | O_TRUNC | O_WRONLY,0600)))<0)
		{
			perror("open can0:\n");			
		}
		else{
		}
		write(fd,rate[rate_cnt],strlen(rate[rate_cnt]));
		close(fd);			
		sleep(1);

		system("ifconfig can0 up");
		sleep(1);
		My_Printf(dug_can,"ifconfig can0 up,rate_cnt:%d,bitrate:%s!\n",rate_cnt,rate[rate_cnt]);

		s_can0 = socket(PF_CAN, SOCK_RAW, CAN_RAW);
		strcpy(ifr0.ifr_name, "can0");
		ret = ioctl(s_can0, SIOCGIFINDEX, &ifr0);
		addr0.can_family = PF_CAN;
		addr0.can_ifindex = ifr0.ifr_ifindex;
		bind(s_can0, (struct sockaddr *)&addr0, sizeof(addr0));
		if (ret < 0) {
			perror("bind can0 failed");
			//return 1;
		}
		ret = setsockopt(s_can0, SOL_CAN_RAW, CAN_RAW_FILTER, &canfilter[0], canfcnt[0] * sizeof(canfilter[0][0]));
		if (ret < 0) {
			My_Printf(dug_can,"setsockopt can0 failed");
			return 1;
		}
		ret=pthread_create(&id_can0_read,NULL,(void *)task_can0_read,NULL);
		if(ret!=0)
			My_Printf(dug_can,"prhtead can0read cread1");
	}

#if 1
	if(can_channel[1].bIsValid)
	{
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
		My_Printf(dug_can,"ifconfig can1 up,rate_cnt:%d,bitrate:%s!\n",rate_cnt,rate[rate_cnt]);
		s_can1 = socket(PF_CAN, SOCK_RAW, CAN_RAW);
		strcpy(ifr1.ifr_name, "can1");
		ret = ioctl(s_can1, SIOCGIFINDEX, &ifr1);
		addr1.can_family = PF_CAN;
		addr1.can_ifindex = ifr1.ifr_ifindex;
		bind(s_can1, (struct sockaddr *)&addr1, sizeof(addr1));
		ret = setsockopt(s_can1, SOL_CAN_RAW, CAN_RAW_FILTER, &canfilter[1], canfcnt[1] * sizeof(canfilter[1][0]));

		ret=pthread_create(&id_can0_read,NULL,(void *)task_can1_read,NULL);
		if(ret!=0)
			My_Printf(dug_can,"prhtead can1read cread1");

	}
#endif

	ret=pthread_create(&id_canTP,NULL,(void *)task_canTP,NULL);
	while(1)
	{
		if(T15_state == 0)
		{
			sleep(1);
			continue;
		}
		if(g_isDownloadCfg)
		{
			sleep(1);
			continue;
		}
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
						//My_Printf(dug_can,"snd currentlid:0x%08x\n",currentlid);
						currentLidlen = can_channel[i].pt_logic_can[j]->pt_signal[k]->ucLidLength;

						buffer[0] = can_channel[i].pt_logic_can[j]->pt_signal[k]->ucRdDatSerID;
						b15765rcv = 0;
						switch(can_channel[i].pt_logic_can[j]->pt_signal[k]->ucRdDatSerID)
						{
							case 0x21:  //read data by LID
							case 0x22:  //read data by LID
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
								My_Printf(dug_can,"CAN_Send15765,ulFlashTxId[%d]:%08x,%02x %02x %02x\n",i,ulFlashTxId[i],buffer[0],buffer[1],buffer[2]);
								break;
							case 0x23:  //read data by MEM
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
								My_Printf(dug_can,"CAN_Send15765,ulFlashTxId[%d]:%08x,:%02x %02x %02x %02x %02x\n",i,ulFlashTxId[i],buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]);
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
		usleep(5000);
	}	
	
}



void SDBufferInit( void )
{
    SD_buffer.queuefull = 0;

    SD_buffer.cnt = 4;
    SD_buffer.max = SD_QUEUE_SIZE-200;
}
BOOL Write2SDBuffer(FILE *fp,uint8_t* buffer, uint16_t len)
{
	uint16_t i;
	static uint32_t nextupdatetime = 1000;	//if timeout, send data to wince,frequence is 200ms
	if(SD_buffer.cnt != 4)
	if((SD_buffer.blocknum >= 200)||(getSystemTime() > nextupdatetime)||(len >= SD_buffer.max -SD_buffer.cnt))
	{			
		nextupdatetime = getSystemTime+200;//if timeout, send data to wince,frequence is 100ms
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

BOOL Write2SDBufferDone(FILE *fp)
{			
	int i;
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
	//write to sdcard;
	fclose(fp);
	
}

void task_sd()
{
	FILE *fp;	
	int isCurRcdFileOpen;
	int iCount;
/*
	fp = fopen(FileDeviceName,"r");
	
	if(fp == NULL)		
	{
		//printf("no FileDeviceName file\n");
		fp = fopen(FileDeviceName,"w");
		{
			memcpy(CAR_ID,"E001",strlen("E001"));	
			//fwrite("E1234",strlen("E1234"),1,fp);
			iCount=fwrite(CAR_ID,1,strlen(CAR_ID),fp);
			//printf("strlen(CAR_ID)=%d\n",strlen(CAR_ID));
			//printf("fwrite iCount=%d\n",iCount);
			fclose(fp);
		}
			
	}
	else
	{
		//iCount=fread(CAR_ID,1,10,fp);
		fgets(CAR_ID,10,fp);
		//printf("fread iCount=%d\n",iCount);
		//printf("strlen(CAR_ID)=%d\n",strlen(CAR_ID));
		fclose(fp);
	}
	
*/	
	pthread_mutex_lock(&g_file_mutex); 
	getFilesRecords();
	getFilesName();
	writeFilesRecords();
	pthread_mutex_unlock(&g_file_mutex); 
	
	while(1)
	{
		sleep(10);
		
		fileTransState = IDLE;

		My_Printf(dug_readcfg,"T15_state:%d\n",T15_state);
		while(T15_state == 0)
		{
			sleep(1);
		}
		My_Printf(dug_readcfg,"T15_state:%d\n",T15_state);
		//printf("$1\n");
		getSDstatus(&sd_total,&sd_used,&sd_free);
		db_sd_free = (double)(sd_free/1024);
		//printf("$2\n");
		creat_FileName(RECORDPATH,record_filename);
		//printf("$3\n");

		My_Printf(dug_readcfg,"record_filename is %s\n",record_filename);		
		//printf("$4\n");

		fp=fopen(record_filename,"w+b");	
		if(fp > 0)
		{
			My_Printf(dug_readcfg,"create file ok,file name=%s\n",record_filename);	
		}
		//printf("$5\n");
		isCurRcdFileOpen = 1;
		
		fwrite(auth_code,509,1,fp);			
		SDBufferInit();
	
		while(T15_state)
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
				My_Printf(dug_can,"can15765buf.cnt:%d,len:%d\n",can15765buf.cnt,can15765buf.qdata[can15765buf.rp].len);
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

void HeartBeatReport(int sockfd,const struct sockaddr_in *addr,int len);

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
	memcpy(&buf[16],&longitude,8);
	memcpy(&buf[24],&latitude,8);
	memcpy(&buf[32],&height,8);
	memcpy(&buf[40],&speed,8);
	
	memcpy(&buf[48],&db_sd_free,8);
	memcpy(&buf[56],&g_edas_state,8);
				

	My_Printf(dug_3g,"3g_latitude  = %f\n",longitude);
	My_Printf(dug_3g,"3g_longitude = %f\n",latitude);
	My_Printf(dug_3g,"3g_speed	  = %f\n",height);
	My_Printf(dug_3g,"3g_height	  = %f\n",speed);
	My_Printf(dug_3g,"3g_sd_free  = %f\n",db_sd_free);

	chksum(buf,DataLen+8);
	cnt = sendto(sockfd,buf,DataLen+9,0,(struct sockaddr *)addr,len);
	My_Printf(dug_3g,"snd heartbeak,cnt:%d\n",cnt);

	if(g_edas_state.state_gps != 2)
		g_edas_state.state_gps = 0;
	if(g_edas_state.state_k != 2)
	{
		if(T15_state==1)
		{
			if(g_edas_state.state_k == 0)
			{
				kline_break_cnt++;
				if(kline_break_cnt > 100)
				{
					My_Printf(dug_sys,"kline_break_reboot!\n");
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				kline_break_cnt = 0;
			}
		}
		g_edas_state.state_k = 0;
		
	}
	if(g_edas_state.state_can0_1939 != 2)
	{
		if(T15_state==1)
		{
			if(g_edas_state.state_can0_1939 == 0)
			{
				can0_1939_break_cnt++;
				if(can0_1939_break_cnt > 100)
				{
					My_Printf(dug_sys,"can0_1939_break_reboot!\n");
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can0_1939_break_cnt = 0;
			}
		}
		g_edas_state.state_can0_1939  = 0;
	}
	if(g_edas_state.state_can0_15765 != 2)
	{
		if(T15_state==1)
		{
			if(g_edas_state.state_can0_15765 == 0)
			{
				can0_15765_break_cnt++;
				if(can0_15765_break_cnt > 100)
				{
					My_Printf(dug_sys,"can0_15765_break_reboot!\n");
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can0_15765_break_cnt = 0;
			}
		}
		g_edas_state.state_can0_15765 = 0;
	}
	if(g_edas_state.state_can1_1939 != 2)
	{
		if(T15_state==1)
		{
			if(g_edas_state.state_can1_1939 == 0)
			{
				can1_1939_break_cnt++;
				if(can1_1939_break_cnt > 100)
				{
					My_Printf(dug_sys,"can1_1939_break_reboot!\n");
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can1_1939_break_cnt = 0;
			}
		}
		g_edas_state.state_can1_1939  = 0;
	}
	if(g_edas_state.state_can1_15765 != 2)
	{
		if(T15_state==1)
		{
			if(g_edas_state.state_can1_15765 == 0)
			{
				can1_15765_break_cnt++;
				if(can1_15765_break_cnt > 100)
				{
					My_Printf(dug_sys,"can1_15765_break_reboot!\n");
					sleep(1);
					system("reboot");
				}
			}
			else
			{
				can1_15765_break_cnt = 0;
			}
		}
		g_edas_state.state_can1_15765 = 0;
	}
	if((T15_state==1)&&(isSaveGps == 1))
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

	   	memcpy(&bps_buf.qdata[bps_buf.wp].data[8],&US_SECOND,4);
		memcpy(&bps_buf.qdata[bps_buf.wp].data[12],&US_MILISECOND,2);
		
		memcpy(&bps_buf.qdata[bps_buf.wp].data[14],&gpsDataLen,2);
		
		memcpy(&bps_buf.qdata[bps_buf.wp].data[16],&longitude,8);
		memcpy(&bps_buf.qdata[bps_buf.wp].data[24],&latitude,8);
		memcpy(&bps_buf.qdata[bps_buf.wp].data[32],&height,8);
		memcpy(&bps_buf.qdata[bps_buf.wp].data[40],&speed,8);	        

        bps_buf.qdata[bps_buf.wp].len = DataLen+4;
        bps_buf.wp++;
        if(bps_buf.wp == GPS_RCV_QUEUE_SIZE)  bps_buf.wp = 0;    
        bps_buf.cnt++;
////////////////////////////////	
}


