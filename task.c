#include <fcntl.h>

#include "task.h"
#include "sd.h"
#include "gps.h"
#include "gpio.h"
#include "common.h"
#include "recfile.h"
#include "readcfg.h"
#include "kline.h"
#include "net3g.h"




void task_recfile(void)
{
	FILE *fp;	
	int isCurRcdFileOpen;
	int iCount;
	unsigned char record_filename[100];

	pthread_mutex_lock(&g_file_mutex); 
	get_recfile_list();
	get_recfile_name();
	save_recfile_list();
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

		getSDstatus(&sd_total,&sd_used,&sd_free);
		db_sd_free = sd_free/1024; 
		creat_FileName(RECORDPATH,record_filename); 
#if dug_readcfg > 0
				printf_va_args("record_filename is %s\n",record_filename);
#endif 
		fp = fopen(record_filename,"w+b");	
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
			if(g_gps_info.cnt)
			{
				Write2SDBuffer(fp,&g_gps_info.qdata[g_gps_info.rp].data[0], g_gps_info.qdata[g_gps_info.rp].len);
				g_gps_info.rp++;
	            if(g_gps_info.rp == GPS_RCV_QUEUE_SIZE)  
					g_gps_info.rp = 0;    
	            g_gps_info.cnt--;
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

	free(gps_ser_buf);
	gps_ser_buf = NULL;
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


static void task_can0_read()
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


static void task_can1_read()
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


void task_can(void)
{
    int ret;
    pthread_t id_can0_read;
	pthread_t id_can1_read;
	pthread_t id_canTP;

	USB_Flash_Status[0]=KWP_IDLE;
	USB_Flash_Status[1]=KWP_IDLE;
		
	if(can_channel[0].bIsValid)
	{
		up_can0();
		ret=pthread_create(&id_can0_read,NULL,(void *)task_can0_read,NULL);
#if dug_can > 0 		
		if(ret!=0)
			printf_va_args("prhtead can0read created");
#endif
	}

	if(can_channel[1].bIsValid)
	{
		up_can1();
		ret=pthread_create(&id_can1_read,NULL,(void *)task_can1_read,NULL);
#if dug_can > 0		
		if(ret!=0)
			printf_va_args("prhtead can1read created");
#endif
	}

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



void task_k(void)
{	
	static int fd_k = -1;
	uint8_t i;
	static uint32_t currentKlinelid = 0;
	static uint8_t ucLogicKIndex = 0;
	uint8_t kcommand[10];
	uint8_t ktest[10];
	int j;
	ssize_t len;
	int kCommandLen;
	int rcvcnt;
	#define BUF_SIZE 255
	unsigned char pBuf[BUF_SIZE];
	unsigned int overtime;
	int Savedatalen;

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
		for(i=0; i<siganl_kline_num; i++)
		{			
			if((kline_config[0].bIsValid)&&(g_sys_info.state_T15==1))
			{
				switch(KlineInitState[0])
				{
					case K_INIT_BREAK:
					{
						memset(pBuf,0,BUF_SIZE);						
						memcpy((void*)kcommand,(const void*)kline_config[0].ucInitFrmData,10);

						if(fd_k > 0)
						{
							close(fd_k);
						}
						fd_k = OpenKline();
						write(fd_k,(void*)kcommand,5);						
						kline_TxState.length = 5;												
						overtime = getSystemTime()+2000;
						
						rcvcnt = read_kfile(fd_k, pBuf, 5, overtime);
						
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
					}
					break;
						
					case K_INIT_OK:
					{
						if((k_rcv_last_time + 5000) < getSystemTime())
						{
							KlineInitState[0] = K_TIMEOUT;							
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
								default:
									break;
							}							
							chksum(kcommand, kCommandLen);
							write(fd_k,(void*)kcommand,kCommandLen+1);													
							kline_siganl[i].dwNextTime = getSystemTime()+kline_siganl[i].wSampleCyc;

							memset(pBuf,0,0xff);
							overtime = getSystemTime()+1000;
										
							rcvcnt = read_kfile(fd_k, pBuf, (kCommandLen+1), overtime);																			

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
						}
					}
					break;
						
					case K_TIMEOUT:
					{
						if(fd_k > 0)
						{
							close(fd_k);
						}
						KlineInitState[0] = K_INIT_BREAK;
						fd_k = 0;
					}
					break;

					default:
					break;						
				}
			}
		}
		usleep(1000);		
	}
}	


void task_heartbeat(void)
{
	while(1)
	{
		while(g_sys_info.state_3g != 1)
		{
			sleep(1);
		}
		sleep(5);
		if(curMsgstate.state == MsgState_rcv)
		{
			Request_collect(sockfd,&g_addr,sizeof(struct sockaddr_in));
		}
		
		sleep(5);
		HeartBeatReport(sockfd,&g_addr,sizeof(struct sockaddr_in));
		sleep(5);
		HeartBeatReport(sockfd,&g_addr,sizeof(struct sockaddr_in));
		sleep(5);
		HeartBeatReport(sockfd,&g_addr,sizeof(struct sockaddr_in));
		sleep(5);
		HeartBeatReport(sockfd,&g_addr,sizeof(struct sockaddr_in));
	}
}

//unsigned char rbuf[MAX_SIZE];

void task_udprcv()
{
	unsigned int n_rcv;	
	MsgInfo msg;
	unsigned char *rbuf = NULL;
	rbuf = (unsigned char *)malloc(1024);
	if(NULL == rbuf)
		exit(1);
	
	while(1)
	{
		while(g_sys_info.state_3g != 1)
		{
			sleep(1);
		}
		
		bzero(rbuf,MAX_SIZE);
		n_rcv = recvfrom(sockfd,rbuf,MAX_SIZE,0,NULL,NULL);		
		g_sys_info.net_stat = 1;
		g_Rcv3gCnt++;		
		if(g_Rcv3gCnt > 0x1FFFFFFF) 
		{
			g_LastRcvCnt = 0;
			g_Rcv3gCnt = 1;
		}
				
		msg.len = n_rcv;
		msg.ucServeId = rbuf[0];
		memcpy(msg.ucBuff,rbuf,n_rcv);

		if(checkdata(rbuf,n_rcv))
		{
			EnQueue(&msg_queue,&msg);
		}
	}

	free(rbuf); 
    	rbuf = NULL; 
}


void task_ChkSndFile(void)
{
	int i;
	char filename[255];
	int state;
	static int noFileTransCnt = 0;
	static int powerOffCnt = 0;

	while(1)
	{		
		sleep(10);		
		state = 0;		
		pthread_mutex_lock(&g_file_mutex); 
		for(i=0;i<filescnt;i++)
		{
			if(filesRecord[i].flag != 2)
			{
				curTransFile = i;
				state = 1;
				break;
			}
		}			
		pthread_mutex_unlock(&g_file_mutex); 

		if(state == 0)
		{
			noFileTransCnt++;
		}
		else
		{
			noFileTransCnt = 0;
		}
		
#if dug_sys > 0 
		printf_va_args("fileTransState:%d,curTransFile :%d,filescnt:%d,state:%d\n",fileTransState,curTransFile,filescnt,state);
#endif 		
		if(g_sys_info.state_T15 == 1)
		{
			powerOffCnt = 0;
		}
		else
		{
			if((noFileTransCnt > 3)&&(fileTransState == IDLE))
			{
				powerOffCnt++;
				sleep(10);
				if(powerOffCnt > 2)
				{
#if dug_sys > 0
					printf_va_args("noFileTrans_reboot!\n");
#endif
					sleep(1);
					system("reboot");
				}
					
			}
		}
		
		if((fileTransState == IDLE)&&(state == 1))
		{
			if(g_isDownloadCfg)
			{
				continue;
			}

#if dug_sys > 0			
			printf_va_args("fileTransState :%d\n",fileTransState);
#endif
			if((curTransFile >= 0)&&(curTransFile <filescnt))
			{				
				memset(curTransFileName,0,255);
				memcpy(curTransFileName,RECORDDIR,strlen(RECORDDIR));
				memcpy(&curTransFileName[strlen(RECORDDIR)],filesRecord[i].filename,strlen(filesRecord[i].filename));

				memset(cur3gfilename,0,100);
				memcpy(cur3gfilename,filesRecord[i].filename,strlen(filesRecord[i].filename));
				cur3gfilename_len = strlen(filesRecord[i].filename);	
#if dug_sys > 0				
				printf_va_args("curMsgstate.state :%d\n",curMsgstate.state);
#endif				
				if(curMsgstate.state == MsgState_rcv)
				{
					fp_curTrans = fopen(curTransFileName , "rb");
					if(fp_curTrans == NULL)
					{
#if dug_sys > 0
						printf_va_args("fopen failed\n");
#endif
					}
					else
					{
						g_fp_curTrans_state = 1;
					}
					//printf_va_args_en(dug_3g,"1223_filename is :%s\n",curTransFileName);
					Request_upfile(fp_curTrans,sockfd,&g_addr,sizeof(struct sockaddr_in));
					fileTransState = REQUEST_UPFILE;
				}
				
			}
		}

	}	
	
}

