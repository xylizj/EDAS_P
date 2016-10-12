#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "task.h"
#include "sd.h"
#include "gps.h"
#include "gpio.h"
#include "led.h"
#include "common.h"
#include "recfile.h"
#include "readcfg.h"
#include "kline.h"
#include "net3g.h"
#include "upload_file.h"


static inline
void doWrite2SDBuffer(FILE *fp)
{
	if(can1939buf[0].cnt)
	{
		Write2SDBuffer(fp,&can1939buf[0].qdata[can1939buf[0].rp].data[0], can1939buf[0].qdata[can1939buf[0].rp].len);
		can1939buf[0].rp++;
		if(can1939buf[0].rp == CAN_RCV1939_QUEUE_SIZE)	
			can1939buf[0].rp = 0;	 
		can1939buf[0].cnt--;
	}
	if(can1939buf[1].cnt)
	{
		Write2SDBuffer(fp,&can1939buf[1].qdata[can1939buf[1].rp].data[0], can1939buf[1].qdata[can1939buf[1].rp].len);
		can1939buf[1].rp++;
		if(can1939buf[1].rp == CAN_RCV1939_QUEUE_SIZE)	
			can1939buf[1].rp = 0;	 
		can1939buf[1].cnt--;
	}
	if(can15765buf.cnt)
	{
#if DEBUG_CAN > 0
		printf_va_args("can15765buf.cnt:%d,len:%d\n",can15765buf.cnt,can15765buf.qdata[can15765buf.rp].len);
#endif
		Write2SDBuffer(fp,&can15765buf.qdata[can15765buf.rp].data[0], can15765buf.qdata[can15765buf.rp].len);
		can15765buf.rp++;
		if(can15765buf.rp == CAN_RCV15765_QUEUE_SIZE)  
			can15765buf.rp = 0;	  
		can15765buf.cnt--;
	}
	if(k15765queue.cnt)
	{
		Write2SDBuffer(fp,&k15765queue.qdata[k15765queue.rp].data[0], k15765queue.qdata[k15765queue.rp].len);
		k15765queue.rp++;
		if(k15765queue.rp == K_RCV15765_QUEUE_SIZE)  
			k15765queue.rp = 0;	
		k15765queue.cnt--;
	}
	if(g_gps_queue.cnt)
	{
		Write2SDBuffer(fp,&g_gps_queue.qdata[g_gps_queue.rp].data[0], g_gps_queue.qdata[g_gps_queue.rp].len);
		g_gps_queue.rp++;
		if(g_gps_queue.rp == GPS_RCV_QUEUE_SIZE)  
			g_gps_queue.rp = 0;	  
		g_gps_queue.cnt--;
	}
	usleep(2000);
}


static
void fill_filerecord_list(char *record_filename)
{
	//pthread_mutex_lock(&g_file_mutex); 
	if(NULL != filesRecord[recfile_info.filescnt].filename)
	{
		memcpy(filesRecord[recfile_info.filescnt].filename,&record_filename[27],strlen(record_filename)-27);
		filesRecord[recfile_info.filescnt].flag = 0;
		recfile_info.filescnt++;
	}
	//pthread_mutex_unlock(&g_file_mutex); 
}


void task_recfile(void)
{
	FILE *fp;	
	char isCurRcdFileOpen;
	//unsigned char record_filename[PATH_AND_FILE_LEN];
	char *record_filename = NULL;//20160928 xyl 

	pthread_mutex_lock(&g_file_mutex); 
	get_recfile_list();
	get_recfile_name();
	save_recfile_list();
	pthread_mutex_unlock(&g_file_mutex); 
	
	while(1)
	{
		sleep(10);		
		upfile_info.fileTransState = IDLE;
		while(g_sys_info.state_T15 == 0)
		{
			sleep(1);
		}

		getSDstatus(&sd_info.sd_total,&sd_info.sd_used,&sd_info.sd_free);
		sd_info.db_sd_free = sd_info.sd_free/1024; 
		record_filename = (char *)malloc(PATH_AND_FILE_LEN);//20160928 xyl add
		if(NULL != record_filename)
		{
			memset(record_filename,0,PATH_AND_FILE_LEN);//20160928 xyl add
			creat_FileName(RECORDPATH,record_filename); 
#if DEBUG_CFG > 0
			printf_va_args("record_filename is %s\n",record_filename);
#endif 
			fp = fopen(record_filename,"w+b");
			if(fp > 0)//20161009 xyl, only if open is ok, must check
			{
#if DEBUG_CFG > 0
				printf_va_args("create file ok,file name=%s\n",record_filename); 			 				 
#endif
				isCurRcdFileOpen = 1;
				if(NULL != auth_code){
						fwrite(auth_code,509,1,fp);			
						free(auth_code);
						auth_code = NULL;
					}
				SDBufferInit();
			
				while(g_sys_info.state_T15)
				{		
					while(g_sys_info.downloading_cfg)
					{
						if(isCurRcdFileOpen == 1)
						{
							fclose(fp);
							isCurRcdFileOpen = 0;
						}
						sleep(1);
					}
					doWrite2SDBuffer(fp);
				}
				if(isCurRcdFileOpen == 1)
				{
					Write2SDBufferDone(fp);
					isCurRcdFileOpen = 0;
				}
				fill_filerecord_list(record_filename);
				free(record_filename);//20160928 xyl add
				record_filename = NULL;//20160928 xyl add
			}
		}
	}
}


void task_gps(void)
{
	int len;
	int fd;	

	char *gps_ser_raw_buf = (char *)malloc(GPS_SERIAL_BUF_LEN);
	if(NULL == gps_ser_raw_buf)
		return;

	fd = openSerial("/dev/ttySP1");
	
	while (1) 
	{
		len = read(fd, gps_ser_raw_buf, GPS_SERIAL_BUF_LEN);	
		usleep(1000);
	
		if(TRUE == process_gps_ser_buf(gps_ser_raw_buf, len))
		{			
			parse_gps_signal();	
		}
	}

	free(gps_ser_raw_buf);
	gps_ser_raw_buf = NULL;
}


void task_wd(void)
{
	while(1)
	{
		edas_wd_on();
		usleep(50000);
		edas_wd_off();
		usleep(50000);
	}
}



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


static 
void task_can0_read()
{
	int ret;
	fd_set rset;
	int i;
	char is15765rcv;
	struct can_frame frdup;

	while(1)
	{
		FD_ZERO(&rset);
		FD_SET(can_struct[0].socket,&rset);
		ret = read(can_struct[0].socket,&frdup,sizeof(frdup));
		led_Rx_blink(5);
		update_time();	

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


static 
void task_can1_read()
{
	int ret;
	fd_set rset;
	int i;
	char is15765rcv;
	struct can_frame frdup;

	while(1)
	{
		FD_ZERO(&rset);
		FD_SET(can_struct[1].socket,&rset);
		ret = read(can_struct[1].socket,&frdup,sizeof(frdup));
		led_Rx_blink(5);
		update_time();	

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
#if DEBUG_CAN > 0 		
		if(ret!=0)
			printf_va_args("prhtead can0read created");
#endif
	}

	if(can_channel[1].bIsValid)
	{
		up_can1();
		ret=pthread_create(&id_can1_read,NULL,(void *)task_can1_read,NULL);
#if DEBUG_CAN > 0		
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
		if(g_sys_info.downloading_cfg)
		{
			sleep(1);
			continue;
		}
		can_info_process();		
		usleep(5000);
	}		
}


static 
void fill_kcommand(uint8_t i, char kcommand[], int *kCommandLen)
{
	kcommand[1] = kline_config[0].ucInitFrmData[1];
	kcommand[2] = kline_config[0].ucInitFrmData[2];
	kcommand[3] = kline_siganl[i]->ucRdDatSerID;
	
	switch(kline_siganl[i]->ucRdDatSerID)
	{
		case 0x23:
			kcommand[0] = 0x85; 								
			kcommand[4] = 0xFF & (kline_siganl[i]->dwLocalID >> 16);
			kcommand[5] = 0xFF & (kline_siganl[i]->dwLocalID >> 8);
			kcommand[6] = 0xFF & (kline_siganl[i]->dwLocalID);		
			kcommand[7] = kline_siganl[i]->ucMemSize;
			*kCommandLen = 8;
			break;
		case 0x21:									
		case 0x22:
			kcommand[0] = 0x82;
			kcommand[4] = 0xFF & kline_siganl[i]->dwLocalID;
			*kCommandLen = 5;
			break;
		default:
			break;
	}	
}


static 
void fill_k15765buf_qdata(uint8_t ucRdDatSerID, unsigned char*pBuf, uint32_t currentKlinelid)
{
	int Savedatalen;

	Savedatalen = kline_RxState.dataLength - 2;
	if(ucRdDatSerID == 0x23)
	{
		Savedatalen++;
	}

	k15765queue.qdata[k15765queue.wp].data[0] = UP_DAQ_SIGNAL;
	k15765queue.qdata[k15765queue.wp].data[1] = SIGNAL_DIAG_KLINE;
	k15765queue.qdata[k15765queue.wp].data[2] = 0xFF & (Savedatalen+17-4); 
	k15765queue.qdata[k15765queue.wp].data[3] = (Savedatalen+17-4) >> 8;

	k15765queue.qdata[k15765queue.wp].data[4] = 0xFF & currentKlinelid;
	k15765queue.qdata[k15765queue.wp].data[5] = 0xFF & (currentKlinelid>>8);
	k15765queue.qdata[k15765queue.wp].data[6] = 0xFF & (currentKlinelid>>16);
	k15765queue.qdata[k15765queue.wp].data[7] = 0xFF & (currentKlinelid>>24);

	memcpy(&k15765queue.qdata[k15765queue.wp].data[8], (void*)&US_SECOND,4);
	memcpy(&k15765queue.qdata[k15765queue.wp].data[12], (void*)&US_MILISECOND,2);


	k15765queue.qdata[k15765queue.wp].data[14] = 0xFF & (Savedatalen);
	k15765queue.qdata[k15765queue.wp].data[15] = Savedatalen >> 8;							
	k15765queue.qdata[k15765queue.wp].data[16] = kline_config[0].ucLogicChanIndex;
	if(ucRdDatSerID == 0x23)
	{								
		(void)memcpy(&k15765queue.qdata[k15765queue.wp].data[17],(const void *)(&pBuf[kline_RxState.headerLength+1]), Savedatalen);//add kline index
	}
	else
	{
		(void)memcpy(&k15765queue.qdata[k15765queue.wp].data[17],(const void *)(&pBuf[kline_RxState.headerLength+2]), Savedatalen);//add kline index
	}
	k15765queue.qdata[k15765queue.wp].len = Savedatalen + 17;
	k15765queue.wp++;
	if(k15765queue.wp == K_RCV15765_QUEUE_SIZE)  
		k15765queue.wp = 0;	
	k15765queue.cnt++;
}

static inline
void print_k_bug(KLINE_INIT_STATE k_state)
{
	switch(k_state)
	{
		case K_INIT_BREAK:
			break;
		case K_INIT_OK:
			printf_va_args("kline recv %d:",kline_RxState.dataLength);
			break;
		case K_TIMEOUT:
			printf_va_args("kline timeout\n");
			break;
		default:
			break;
	}
}


static
void k_info_process(void)
{		
	unsigned char i;
	static int fd_k = -1;	
	static unsigned int currentKlinelid = 0;
	//static uint8_t ucLogicKIndex = 0;
	char kcommand[10];
	int kCommandLen;
	int rcvcnt;	
	static unsigned char *pBuf;//unsigned char pBuf[BUF_SIZE];
	unsigned int overtime;	
	static unsigned int k_rcv_last_time = 0;
	
	for(i=0; i<k_sig_num; i++)
	{
		if((kline_config[0].bIsValid)&&(g_sys_info.state_T15==1))
		{
#if (DEBUG_K > 0)
			KLINE_INIT_STATE k_state = KlineInitState[0];
#endif
			switch(KlineInitState[0])
			{
				case K_INIT_BREAK:
				{
					pBuf = (unsigned char*)malloc(KBUF_SIZE);
					if(NULL == pBuf)
						break;
					memset(pBuf,0,KBUF_SIZE);						
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

					if(NULL == kline_siganl[i])
						break;
					
					if(kline_siganl[i]->dwNextTime < getSystemTime())
					{
						currentKlinelid = kline_siganl[i]->dwLocalID;
						//ucLogicKIndex = kline_siganl[i].ucLogicChanIndex;							
						fill_kcommand(i, kcommand, &kCommandLen);								
						chksum(kcommand, kCommandLen);
						write(fd_k,(void*)kcommand,kCommandLen+1);													
						kline_siganl[i]->dwNextTime = getSystemTime()+kline_siganl[i]->wSampleCyc;
	
						memset(pBuf,0,0xff);
						overtime = getSystemTime()+1000;									
						rcvcnt = read_kfile(fd_k, pBuf, (kCommandLen+1), overtime);	
						if(overtime < getSystemTime())
						{
							KlineInitState[0] = K_TIMEOUT;
							break;
						}							
						handl_K_rcvdata(pBuf);
						update_time();
						led_k_blink(2);
						fill_k15765buf_qdata(kline_siganl[i]->ucRdDatSerID, pBuf, currentKlinelid);			
						k_rcv_last_time = getSystemTime();	
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
					free(pBuf);
					pBuf = NULL;
				}
				break;
	
				default:
				break;						
			}
#if DEBUG_K > 0
			print_k_bug(k_state);
#endif
		}
	}
}			




void task_k(void)
{	
	while(1)
	{
		if(g_sys_info.state_T15 == 0)
		{
			sleep(1);
			continue;
		}
		if(g_sys_info.downloading_cfg)
		{
			sleep(1);
			continue;
		}

		k_info_process();		
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
			Request_collect(net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
		}
		
		sleep(5);
		HeartBeatReport(net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
		sleep(5);
		HeartBeatReport(net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
		sleep(5);
		HeartBeatReport(net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
		sleep(5);
		HeartBeatReport(net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
	}
}

//unsigned char rbuf[MAX_SIZE];

void task_udprcv(void)
{
	unsigned int n_rcv;	
	MsgInfo msg;
	unsigned char *rbuf = NULL;
	
	while(1)
	{
		while(g_sys_info.state_3g != 1)
		{
			sleep(1);
		}
		
		rbuf = (unsigned char *)malloc(MAX_SIZE);
		if(NULL != rbuf)
		{
			bzero(rbuf,MAX_SIZE);
			n_rcv = recvfrom(net3g.sockfd,rbuf,MAX_SIZE,0,NULL,NULL);		
			g_sys_info.net_stat = 1;
			net3g.rcvcnt++;		
			if(net3g.rcvcnt > 0x1FFFFFFF) 
			{
				net3g.rcvcnt_last = 0;
				net3g.rcvcnt = 1;
			}
					
			msg.len = n_rcv;
			msg.ucServeId = rbuf[0];
			memcpy(msg.ucBuff,rbuf,n_rcv);

			if(checkdata(rbuf,n_rcv))
			{
				EnQueue(&curMsgstate.queue,&msg);
			}
			
			free(rbuf); 
		    rbuf = NULL; 
		}
	}
}

static 
void t15_filetrans(bool nofile_tran)
{
	static int powerOffDeb = 0;
	
	if(g_sys_info.state_T15 == 1)
	{
		powerOffDeb = 0;
	}
	else
	{
		static unsigned char noFileTransCnt = 0;
		if(nofile_tran)
			noFileTransCnt ++;
		else
			noFileTransCnt = 0;
		if((noFileTransCnt >= 3)&&(upfile_info.fileTransState == IDLE))
		{
			powerOffDeb++;
			sleep(10);
			if(powerOffDeb >= 2)
			{
#if DEBUG_SYS > 0
				printf_va_args("No file to transport,shut down system!\n");
#endif				
				edas_power_off();//sleep(1);system("reboot");//20160927
			}					
		}
	}
}

static
void prepare_request(int idx_transfile)
{
	if((upfile_info.curTransFileIndex >= 0)&&(upfile_info.curTransFileIndex <recfile_info.filescnt) && (NULL != filesRecord[idx_transfile].filename))
	{				
		memset(upfile_info.curTransFileName,0,100);
		memcpy(upfile_info.curTransFileName,RECORDDIR,strlen(RECORDDIR));
		memcpy(&upfile_info.curTransFileName[strlen(RECORDDIR)],filesRecord[idx_transfile].filename,strlen(filesRecord[idx_transfile].filename));
		memset(upfile_info.upsuccess_filename,0,100);
		memcpy(upfile_info.upsuccess_filename,filesRecord[idx_transfile].filename,strlen(filesRecord[idx_transfile].filename));
		upfile_info.upsuccess_filename_len = strlen(filesRecord[idx_transfile].filename);	
#if DEBUG_SYS > 0				
		printf_va_args("curMsgstate.state :%d\n",curMsgstate.state);
#endif				
		if(curMsgstate.state == MsgState_rcv)
		{
			upfile_info.fp_curTrans = fopen(upfile_info.curTransFileName , "rb");
			if(upfile_info.fp_curTrans == NULL)
			{				
				upfile_info.fp_curTrans_state = 0;
#if DEBUG_SYS > 0
				printf_va_args("fopen failed\n");
#endif
			}
			else
			{
				upfile_info.fp_curTrans_state = 1;
			}
			Request_upfile(upfile_info.fp_curTrans,net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
			upfile_info.fileTransState = REQUEST_UPFILE;
		}				
	}
}


void task_ChkSndFile(void)
{
	int idx_transfile;
	bool nofile_tran;

	while(1)
	{		
		sleep(10);		
		nofile_tran = TRUE;		
		pthread_mutex_lock(&g_file_mutex); 
		for(idx_transfile=0; idx_transfile<recfile_info.filescnt; idx_transfile++)
		{
			if(filesRecord[idx_transfile].flag != 2)
			{
				upfile_info.curTransFileIndex = idx_transfile;
				nofile_tran = FALSE;
				break;
			}
		}			
		pthread_mutex_unlock(&g_file_mutex);		
#if DEBUG_SYS > 0 
		printf_va_args("upfile_info.fileTransState:%d,upfile_info.curTransFileIndex :%d,recfile_info.filescnt:%d,nofile_tran:%d\n",upfile_info.fileTransState,upfile_info.curTransFileIndex,recfile_info.filescnt,nofile_tran);
#endif 		
		t15_filetrans(nofile_tran);

		if(nofile_tran){
			continue;//skip code below, do next loop
		}
		if(g_sys_info.downloading_cfg){
			continue;
		}
		if(upfile_info.fileTransState == IDLE){			
			prepare_request(idx_transfile);
		}
	}		
}


void task_handle_msg(void)
{
	MsgInfo msg;
	
	while(1)
	{
		if(OK == DeQueue(&curMsgstate.queue,&msg))
		{
			if(curMsgstate.msgid == msg.ucServeId)
			{
				curMsgstate.state = MsgState_rcv;
				curMsgstate.rx_to_cnt = 0;
				curMsgstate.reSndCnt = 0;				
			}

			switch(msg.ucServeId)
			{
				case MSGID_SERVER_TO_EDAS_RESP_CONNECT:		
					doResponseConnect(&msg);
					break;
				case MSGID_SERVER_TO_EDAS_RESP_UPLOAD_FILE:
					doRspUploadFileRes(&msg);									
					break;
				case MSGID_SERVER_TO_EDAS_RESP_CHECK_DATA:
					doRspCheckUploadData(&msg);
					break;
				case MSGID_SERVER_TO_EDAS_RESP_UPLOAD_CPLT:
					doRspUploadDone(&msg);		
					break;
				case MSGID_SERVER_TO_EDAS_REQ_DOWNLOAD_CFG:
					doRspReqDownloadCfg(&msg);
					break;
				case MSGID_SERVER_TO_EDAS_DOWNLOAD_CFGFILE:
					doRspDownloadCfgFile(&msg);
					break;
				case MSGID_SERVER_TO_EDAS_CHECK_DOWNLOADFILE:
					doRspCheckCfgFile(&msg);
					break;
				case MSGID_SERVER_TO_EDAS_REPORT_DOWNLOADDONE:
					doRspReportCfgFileDone(&msg);
					break;					
				default: 
					break;
			}
		}
		else
		{
			usleep(10000);
		}
	}
}


void task_check_msgrx(void)
{
	while(1)
	{	
		if(curMsgstate.state == MsgState_snd)
		{
			sleep(1);
			curMsgstate.rx_to_cnt++;

			if(curMsgstate.rx_to_cnt >= 10)
			{				
				curMsgstate.rx_to_cnt = 0;
				sendUdpMsg(net3g.sockfd,&net3g.addr,curMsgstate.buf,curMsgstate.len,0);
				curMsgstate.reSndCnt++;
				
				if(curMsgstate.msgid == MSGID_SERVER_TO_EDAS_RESP_CONNECT)
				{
					//is3gvalid = 0;
				}
				if(curMsgstate.reSndCnt >= 3)
				{
					curMsgstate.state = MsgState_rcv;
					upfile_info.fileTransState = IDLE;
				}
			}
		}
		else
		{	
			sleep(1);
		}
	}
}

