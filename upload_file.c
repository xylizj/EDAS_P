#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "common.h"
#include "upload_file.h"
#include "queue.h"


FILE *fp_curfile_t;
volatile int g_FileSizeofCfg;
char g_cfgFile[5120];
int g_cfgFilecnt[10] = {0};
volatile int g_cfgFileblock = 0;
FILE *g_fp_cfg; 
volatile int g_fp_cfg_state = 0;


struct sockaddr_in g_addr;

unsigned int flag_rcvd;
unsigned int req_rcv;

unsigned char rbuf[MAX_SIZE];
unsigned int n_rcv;


volatile int filesize;
volatile unsigned int g_read_cur_offset;

//int DeviceID= 0x12345678;
char CarNum[] = "ES0123456789";

volatile unsigned int OffsetList[20];
volatile unsigned int OffsetCnt;
volatile unsigned int OffsetState; //0x01: to be translate;0x02:lost offset;0x03:all lost;0x04:file end
volatile unsigned int OffsetCur;

char testbuf[] = {0x01,0x02,0x03,0x04,0x05,0x06,0x00,0x00,0x0a};

int msgRxTimeoutCnt = 0;


void sendUdpMsg(char *buf,int len,unsigned char rxMsgId)
{	
	if(rxMsgId != 0)
	{
		msgRxTimeoutCnt = 0;
		curMsgstate.msgid = rxMsgId;	
		curMsgstate.state = MsgState_snd;
		curMsgstate.len = len;
		memcpy(curMsgstate.buf,buf,len);
	}
	sendto(sockfd,buf,len,0,(struct sockaddr *)&g_addr,sizeof(struct sockaddr));
}


int Request_collect(int sockfd,const struct sockaddr_in *addr,int len)
{
	unsigned int i,s;
	int res = 0;
	char buf[MAX_SIZE];
	
	char Type = 0x1C;
	char Reserve = 0x00;
	unsigned short DataLen = 20;
	unsigned int n;
	int cnt;
	//int Chksum = 0;

	buf[0] = Type;
	buf[1] = Reserve;
	memcpy(&buf[2],&DeviceID,4);
	memcpy(&buf[6],&DataLen,2);
	memcpy(&buf[8],CarNum,strlen(CarNum));
	chksum(buf,28);
	//printf_va_args_en(dug_3g,"1223Request_collect \n");
	sendUdpMsg(buf,29,MSGID_SERVER_TO_EDAS_RESP_CONNECT);	
	return 1;
}


int Request_upfile(FILE *fp,int sockfd,const struct sockaddr_in *addr,int len)
{
	int cnt;
	char buf[MAX_SIZE];	
	char Type = 0x2C;
	char Reserve = 0x00;
	unsigned short DataLen = 4+strlen(cur3gfilename);

	fseek(fp,0,SEEK_END);	
	filesize= ftell(fp);	
	rewind(fp);
	g_read_cur_offset = 0;

	buf[0] = Type;
	buf[1] = Reserve;
	memcpy(&buf[2], (void*)&DeviceID,4);
	memcpy(&buf[6], (void*)&DataLen,2);
	memcpy(&buf[8], (void*)&filesize,8);
	memcpy(&buf[12], (void*)cur3gfilename,strlen(cur3gfilename));

	chksum(buf,DataLen+8);
	sendUdpMsg(buf,DataLen+9,MSGID_SERVER_TO_EDAS_RESP_UPLOAD_FILE);
	//printf_va_args_en(dug_3g,"Request_upfile ,len:%d\n",len);
	return 1;
} 

void UploadFile(FILE *fp,int sockfd,const struct sockaddr_in *addr,int len)
{
	unsigned int i;
	char buf[MAX_SIZE];
	unsigned int n_group;
	unsigned short DataLen;
	unsigned int FileOffset;
	unsigned int filetrans;
	
	char Type = 0x3C;
	char Reserve = 0x00;
	
	if(OffsetState == 2)
	{
		for(i=0;i<OffsetCnt;i++)
		{
			DataLen = 512+4;
			FileOffset = OffsetList[i];
			
			buf[0] = Type;
			buf[1] = Reserve;
			memcpy(&buf[2],&DeviceID,4);
			memcpy(&buf[6],&DataLen,2);
			memcpy(&buf[8],&FileOffset,4);

			//fseek(fp,FileOffset,SEEK_SET);
			
			fseek(fp,FileOffset-g_read_cur_offset,SEEK_CUR);// you hua offset
			g_read_cur_offset = FileOffset;// you hua offset
			fread(&buf[12],512,1,fp);
			g_read_cur_offset += 512;  // you hua offset
			
			chksum(buf,512+12);
			sendUdpMsg(buf,512+13,0);
			//sendto(sockfd,buf,512+13,0,(struct sockaddr *)addr,len);
			usleep(10000);
		}
	}
	else if((OffsetState == 1)||(OffsetState == 3))
	{		
		n_group = (filesize - OffsetCur)/512;

		if((filesize - OffsetCur)%512) n_group++;
		if(n_group>20) n_group = 20;
		
		for(i=0;i<n_group;i++)
		{
			filetrans = filesize-OffsetCur;
			if(filetrans > 512) filetrans = 512;
			FileOffset = OffsetCur;
			OffsetCur += filetrans;			
			
			buf[0] = Type;
			buf[1] = Reserve;			
			memcpy(&buf[2],&DeviceID,4);
			DataLen = filetrans+4;
			memcpy(&buf[6],&DataLen,2);
			memcpy(&buf[8],&FileOffset,4);			

			//fseek(fp,FileOffset,SEEK_SET);	
			fseek(fp,FileOffset-g_read_cur_offset,SEEK_CUR);// you hua offset
			g_read_cur_offset = FileOffset;// you hua offset
			fread(&buf[12],filetrans,1,fp);
			g_read_cur_offset += filetrans;  // you hua offset
			
			chksum(buf,filetrans+12);
			
			sendUdpMsg(buf,filetrans+13,0);				
			usleep(10000);			
		}
	}
} 

int CheckFile(int sockfd,const struct sockaddr_in *addr,int len)
{
	unsigned int i;
	int cnt;
	char buf[MAX_SIZE];
	
	char Type = 0x4C;
	char Reserve = 0x00;
	unsigned short DataLen = 0;
	unsigned short rcvlen;
	int res;	

	buf[0] = Type;
	buf[1] = Reserve;
	memcpy(&buf[2],&DeviceID,4);
	memcpy(&buf[6],&DataLen,2);
	chksum(buf,8);
	
	sendUdpMsg(buf,9,MSGID_SERVER_TO_EDAS_RESP_CHECK_DATA);	
	return 1;
} 

int TransferDone(int sockfd,const struct sockaddr_in *addr,int len)
{

	char buf[MAX_SIZE];
	
	char Type = 0x5C;
	char Reserve = 0x00;
	unsigned short DataLen = 0;
	int cnt;

	buf[0] = Type;
	buf[1] = Reserve;
	memcpy(&buf[2],&DeviceID,4);
	memcpy(&buf[6],&DataLen,2);	
	chksum(buf,8);
	
	sendUdpMsg(buf,9,MSGID_SERVER_TO_EDAS_RESP_UPLOAD_CPLT);
	return 1;
} 


void task_check_msgrx()
{
	while(1)
	{	
		if(curMsgstate.state == MsgState_snd)
		{
			sleep(1);
			msgRxTimeoutCnt++;

			if(msgRxTimeoutCnt >= 10)
			{				
				msgRxTimeoutCnt = 0;
				sendUdpMsg(curMsgstate.buf,curMsgstate.len,0);
				curMsgstate.reSndCnt++;
				
				if(curMsgstate.msgid == MSGID_SERVER_TO_EDAS_RESP_CONNECT)
				{
					is3gvalid = 0;
					//printf_va_args_en(dug_3g,"is3gvalid is %d\n",is3gvalid);
				}
				if(curMsgstate.reSndCnt >= 3)
				{
					curMsgstate.state = MsgState_rcv;
					fileTransState = IDLE;
				}
			}
		}
		else
		{	
			sleep(1);
		}
	}
}

void doResponseConnect( MsgInfo *msg)
{
	int i;	
	is3gvalid = 1;
	
}

void doRspUploadFileRes( MsgInfo *msg)
{
	int i;	

	if(fileTransState == REQUEST_UPFILE)
	{
		fileTransState = UPLOADING_FILE;	
		OffsetState = 0xFF;
		if(msg->ucBuff[8] == 0x01)
		{
			memcpy((void*)&OffsetList[0], (void*)&(msg->ucBuff[9]), 4);
			OffsetCnt = 1;
			OffsetState = 1;
			OffsetCur = OffsetList[0];

			UploadFile(fp_curTrans,sockfd,&g_addr,sizeof(struct sockaddr_in));
			fileTransState = CHECK_FILE;

			CheckFile(sockfd,&g_addr,sizeof(struct sockaddr_in));
		}	
	}
}
void saveUploadSuccessFileList(char *filePathName,int size)
{
	FILE *fp;
	char filelen[15]={0};

	itoa(size,filelen,10);
	
	fp = fopen(UPLOADEDFILELIST,"a+");
	fwrite(filePathName,strlen(filePathName),1,fp);
	fwrite("	",strlen("	"),1,fp);
	fwrite(filelen,strlen(filelen),1,fp);
	fwrite("\n",strlen("\n"),1,fp);
	fflush(fp);
	fclose(fp);
	//printf_va_args_en(dug_3g,"saveUploadSuccessFileList :%s \n",filePathName);
}

void doRspCheckUploadData( MsgInfo *msg)
{
	int i;
	unsigned short rcvlen;
	char filename[300];
	
	if(fileTransState == CHECK_FILE)
	{			
		OffsetState = msg->ucBuff[8];
		memcpy(&rcvlen,&(msg->ucBuff[6]),2);

		switch(OffsetState)
		{		
			case 1:
			case 3:
				memcpy((void*)&OffsetCur,&(msg->ucBuff[9]),4);
				if(OffsetCur >= filesize)
				{
					//OffsetState = 4;//file end	
					fileTransState = TRANS_DONE;	
					TransferDone(sockfd,&g_addr,sizeof(struct sockaddr_in));

					fileTransState = TRANS_STOP;
					//if(g_fp_curTrans_state == 1)
					//{
						fflush(fp_curTrans);
						fclose(fp_curTrans);
						g_fp_curTrans_state = 0;
					//}
					remove(curTransFileName);					
					saveUploadSuccessFileList(cur3gfilename,filesize);
					
					filesRecord[curTransFile].flag = 2;
					fileTransState = IDLE;
					writeFilesRecords();
				}
				else
				{
					UploadFile(fp_curTrans,sockfd,&g_addr,sizeof(struct sockaddr_in));
					fileTransState = CHECK_FILE;	
					usleep(100000);
					CheckFile(sockfd,&g_addr,sizeof(struct sockaddr_in));
					usleep(100000);
				}
				break;
			case 2:
				OffsetCnt=(rcvlen-1)/4;
				
				for(i=0;i<OffsetCnt;i++)
				{
					memcpy((void*)&OffsetList[i],&(msg->ucBuff[9+4*i]),4);
					fileTransState = UPLOADING_FILE;	
				}	
				
				UploadFile(fp_curTrans,sockfd,&g_addr,sizeof(struct sockaddr_in));
				fileTransState = CHECK_FILE;
				usleep(100000);
				CheckFile(sockfd,&g_addr,sizeof(struct sockaddr_in));
				usleep(100000);
				break;	
			case 4:
				fileTransState = TRANS_STOP;
				if(g_fp_curTrans_state==1)
				{
					fflush(fp_curTrans);
					fclose(fp_curTrans);
					g_fp_curTrans_state = 0;
				}
				fileTransState = IDLE;
				break;
		}
	}
	
}
void doRspUploadDone( MsgInfo *msg)
{
	/*if(fileTransState == TRANS_DONE)
	{
		filesRecord[curTransFile].flag = 2;
		//fileTransState = TRANS_STOP;	
		if(g_fp_curTrans_state == 1)
		{
			g_fp_curTrans_state = 0;
			fclose(fp_curTrans);
		}
		writeFilesRecords();
		fileTransState = IDLE;
		My_Printf(dug_3g,"doRspUploadDone_dodone\n");
	}
	*/
}
void doRspReqDownloadCfg( MsgInfo *msg)
{

	char buf[MAX_SIZE];
	
	char Type = 0xE1;
	char Reserve = 0x00;
	unsigned short DataLen = 5;
	int cnt;
	g_isDownloadCfg = TRUE;
	
	//memcpy(&g_FileSizeofCfg,&(msg->ucBuff[8]),4);
	memcpy((void*)&g_FileSizeofCfg,&(msg->ucBuff[8+4]),4);
	
	g_cfgFileblock = (g_FileSizeofCfg/512);
	if(g_FileSizeofCfg%512) g_cfgFileblock++;
	
	//printf("g_FileSizeofCfg:%d,g_cfgFileblock:%d\n",g_FileSizeofCfg,g_cfgFileblock);
	if(g_fp_cfg_state == 0)
	{
		g_fp_cfg = fopen(EDASCFG_TMP,"wb");
		//printf("fopen EDASCFG_TMP\n");
		g_fp_cfg_state = 1;
	}	

	buf[0] = Type;
	buf[1] = Reserve;
	memcpy(&buf[2],&DeviceID,4);
	memcpy(&buf[6],&DataLen,2);	
	buf[8] = 0x01;
	memset(&buf[9],0,4);	
	chksum(buf,DataLen+8);
	
	sendUdpMsg(buf,DataLen+9,0xE1);
	//printf("??????\n");
		
}
void doRspDownloadCfgFile( MsgInfo *msg)
{
	int i;
	short datalen;
	int fileoffset;
	int blocklen;
	int blocknum;



	memcpy(&datalen,&(msg->ucBuff[6]),2);
	//memcpy(&fileoffset,&(msg->ucBuff[8]),4);
	memcpy(&fileoffset,&(msg->ucBuff[8+4]),4);

	//printf("datalen:%d\n",datalen);
	blocknum = (fileoffset/512);
	//printf("fileoffset:%d\n",fileoffset);
	//printf("blocknum:%d\n",blocknum);
	//memcpy(&g_cfgFile[blocknum],&(msg->ucBuff[12]),datalen-4);
	memcpy(&g_cfgFile[fileoffset],&(msg->ucBuff[12+4]),datalen-4-4);
	g_cfgFilecnt[blocknum] = 1;
	
	//printf("doRspDownloadCfgFile ok\n");
	
}
void doRspCheckCfgFile( MsgInfo *msg)
{
	int i;
	char buf[MAX_SIZE];
	
	char Type = 0xE3;
	char Reserve = 0x00;
	unsigned short DataLen = 5;
	char respCode = 0x01;


	for(i=0;i<g_cfgFileblock;i++)
	{
		if(g_cfgFilecnt[i] == 0)
			respCode = 0x03;
	}

		
	buf[0] = Type;
	buf[1] = Reserve;
	memcpy(&buf[2],&DeviceID,4);
	memcpy(&buf[6],&DataLen,2);	
	buf[8] = respCode;
	memset(&buf[9],0,4);	
	chksum(buf,DataLen+8);
	
	sendUdpMsg(buf,DataLen+9,0xE3);
	sleep(5);
	if(respCode == 0x01)
	{
		//printf("doRspCheckCfgFile success,wait for reboot!\n");
		fwrite(g_cfgFile,g_FileSizeofCfg,1,g_fp_cfg);
		fclose(g_fp_cfg);
		sleep(1);
		//system("rm /media/sd-mmcblk0p1/EDAS_P_CFG.ers");
		remove("/media/sd-mmcblk0p1/EDAS_P_CFG.ers");
		sleep(1);
		system("mv /EDAS_P_CFG.ers /media/sd-mmcblk0p1/");		
		system("rm /media/sd-mmcblk0p1/record/*");
		sleep(1);
		
		//printf_va_args_en(dug_sys,"update_EDAS_P_CFG.ers_reboot!\n");
		sleep(1);
		system("reboot");
	}
}
void doRspReportCfgFileDone( MsgInfo *msg)
{
	
}



void task_handle_msg()
{
	int i;
	MsgInfo msg;
	unsigned short rcvlen;
	is3gvalid = 1;
	while(1)
	{
		if(OK == DeQueue(&msg_queue,&msg))
		{
			if(curMsgstate.msgid == msg.ucServeId)
			{
				curMsgstate.state = MsgState_rcv;
				msgRxTimeoutCnt = 0;
				curMsgstate.reSndCnt = 0;
				
			}
			for(i=0;i<msg.len;i++)
			{
				//printf_va_args_en(dug_3g," %02x",msg.ucBuff[i]);
			}
			//printf_va_args_en(dug_3g,"\n");

			switch(msg.ucServeId)
			{
				case MSGID_SERVER_TO_EDAS_RESP_CONNECT:		
					//printf_va_args_en(dug_3g,"doResponseConnect\n");
					//printf("doResponseConnect\n");
					doResponseConnect(&msg);
					break;
				case MSGID_SERVER_TO_EDAS_RESP_UPLOAD_FILE:
					//printf_va_args_en(dug_3g,"doRspUploadFileRes\n");
					doRspUploadFileRes(&msg);									
					break;
				case MSGID_SERVER_TO_EDAS_RESP_CHECK_DATA:
					//printf_va_args_en(dug_3g,"doRspCheckUploadData\n");
					doRspCheckUploadData(&msg);
					//printf_va_args_en(dug_3g,"doRspCheckUploadData,fileTransState:%d\n",fileTransState);
					break;
				case MSGID_SERVER_TO_EDAS_RESP_UPLOAD_CPLT:
					//printf_va_args_en(dug_3g,"doRspUploadDone,fileTransState:%d\n",fileTransState);
					doRspUploadDone(&msg);		
					break;
					
				case MSGID_SERVER_TO_EDAS_REQ_DOWNLOAD_CFG:
					//printf("REQ_DOWNLOAD_CFG \n");
					doRspReqDownloadCfg(&msg);
					break;
				case MSGID_SERVER_TO_EDAS_DOWNLOAD_CFGFILE:
					//printf("EDAS_DOWNLOAD_CFGFILE \n");
					doRspDownloadCfgFile(&msg);
					break;
				case MSGID_SERVER_TO_EDAS_CHECK_DOWNLOADFILE:
					//printf("CHECK_DOWNLOADFILE \n");
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

int checkdata(unsigned char *buf,unsigned int n)
{
	int i;
	unsigned int sum;
	unsigned char result;
	sum = 0;
	for(i=0;i<n-1;i++)
	{
		sum += buf[i];
	}
	result = 0xFF & sum;
	if(buf[n-1] == result)
	{
		return 1;
	}
	else
	{
		//printf_va_args_en(dug_3g,"3g rcv check error\n");
		return 0;
	}
}
void task_udprcv()
{
	int i;
	MsgInfo msg;
	while(1)
	{
		while(g_edas_state.state_3g != 1)
		{
			sleep(1);
		}
		
		req_rcv = 0;			
		n_rcv = 0;
		bzero(rbuf,MAX_SIZE);
		n_rcv=recvfrom(sockfd,rbuf,MAX_SIZE,0,NULL,NULL);		
		g_edas_state.net_stat = 1;
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
}

int uploadingfile(FILE *fp,int sockfd,const struct sockaddr_in *addr,int len)
{
	int res;
	if(Request_collect(sockfd,addr,len))
	{
		if(Request_upfile(fp,sockfd,addr,len))
		{
			while(OffsetState != 4)
			{			
				if((fp_curfile_t == fp)&&(g_edas_state.state_T15))
				{
					return 0;
				}
				UploadFile(fp,sockfd,addr,len);
				
				if(CheckFile(sockfd,addr,len))
				{
					res = 0;
					return res;
				}		
				
			}
			
			if(TransferDone(sockfd,addr,len))
			{
				return 1;
			}
			
		}
	}

	return 0;
}












