#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "common.h"
#include "recfile.h"
#include "net3g.h"
#include "upload_file.h"



struct _download_cfgfile_info cfgfile_info;
struct _upload_file_info upfile_info;






void sendUdpMsg(int sockfd,const struct sockaddr_in *addr,char *buf,int len,unsigned char rxMsgId)
{	
	if(rxMsgId != 0)
	{
		curMsgstate.rx_to_cnt = 0;
		curMsgstate.msgid = rxMsgId;	
		curMsgstate.state = MsgState_snd;
		curMsgstate.len = len;
		memcpy(curMsgstate.buf,buf,len);
	}
	sendto(sockfd,buf,len,0,(struct sockaddr *)&addr,sizeof(struct sockaddr));
}


int Request_collect(int sockfd,const struct sockaddr_in *addr,int len)
{
	unsigned short DataLen = 20;
	char *buf;//char buf[MAX_SIZE];
	
	buf = (char *)malloc(MAX_SIZE);
	if(NULL == buf){
		return 0;//ToDo
	}

	buf[0] = 0x1C;
	buf[1] = 0x00;
	memcpy(&buf[2],(void *)&g_sys_info.DeviceID,4);
	memcpy(&buf[6],&DataLen,2);
	memcpy(&buf[8],g_sys_info.CAR_ID,strlen(g_sys_info.CAR_ID));
	chksum(buf,28);
	sendUdpMsg(sockfd, addr, buf,29,MSGID_SERVER_TO_EDAS_RESP_CONNECT);	
	free(buf);
	buf = NULL;

	return 1;
}


int Request_upfile(FILE *fp,int sockfd,const struct sockaddr_in *addr,int len)
{
	unsigned short DataLen;
	char *buf;//char buf[MAX_SIZE];	
	buf = (char *)malloc(MAX_SIZE);
	if(NULL == buf){
		return 0;//ToDo
	}

	DataLen = 4+strlen(upfile_info.upsuccess_filename);
	fseek(fp,0,SEEK_END);	
	upfile_info.filesize= ftell(fp);	
	rewind(fp);
	upfile_info.read_cur_offset = 0;

	buf[0] = 0x2C;
	buf[1] = 0x00;
	memcpy(&buf[2], (void*)&g_sys_info.DeviceID,4);
	memcpy(&buf[6], (void*)&DataLen,2);
	memcpy(&buf[8], (void*)&upfile_info.filesize,8);
	memcpy(&buf[12], (void*)upfile_info.upsuccess_filename,strlen(upfile_info.upsuccess_filename));

	chksum(buf,DataLen+8);
	sendUdpMsg(sockfd, addr, buf,DataLen+9,MSGID_SERVER_TO_EDAS_RESP_UPLOAD_FILE);
	free(buf);
	buf = NULL;
	
	return 1;
} 

void UploadFile(FILE *fp,int sockfd,const struct sockaddr_in *addr,int len)
{
	unsigned int i;
	unsigned int n_group;
	unsigned short DataLen;
	unsigned int FileOffset;
	unsigned int filetrans;
	char *buf;//char buf[MAX_SIZE];	
	buf = (char *)malloc(MAX_SIZE);
	if(NULL == buf){
		//ToDo
	}

	if(upfile_info.OffsetState == 2)
	{
		for(i=0; i<upfile_info.OffsetCnt; i++)
		{
			DataLen = 512+4;
			FileOffset = upfile_info.OffsetList[i];
			
			buf[0] = 0x3C;
			buf[1] = 0x00;
			memcpy(&buf[2],(void *)&g_sys_info.DeviceID,4);
			memcpy(&buf[6],&DataLen,2);
			memcpy(&buf[8],&FileOffset,4);

			//fseek(fp,FileOffset,SEEK_SET);
			
			fseek(fp,FileOffset-upfile_info.read_cur_offset,SEEK_CUR);// you hua offset
			upfile_info.read_cur_offset = FileOffset;// you hua offset
			fread(&buf[12],512,1,fp);
			upfile_info.read_cur_offset += 512;  // you hua offset
			
			chksum(buf,512+12);
			sendUdpMsg(sockfd, addr, buf,512+13,0);
			//sendto(sockfd,buf,512+13,0,(struct sockaddr *)addr,len);
			usleep(10000);
		}
	}
	else if((upfile_info.OffsetState == 1)||(upfile_info.OffsetState == 3))
	{		
		n_group = (upfile_info.filesize - upfile_info.OffsetCur)/512;

		if((upfile_info.filesize - upfile_info.OffsetCur)%512) 
			n_group++;
		if(n_group>20) 
			n_group = 20;
		
		for(i=0;i<n_group;i++)
		{
			filetrans = upfile_info.filesize-upfile_info.OffsetCur;
			if(filetrans > 512) filetrans = 512;
			FileOffset = upfile_info.OffsetCur;
			upfile_info.OffsetCur += filetrans;			
			
			buf[0] = 0x3C;
			buf[1] = 0x00;			
			memcpy(&buf[2],(void *)&g_sys_info.DeviceID,4);
			DataLen = filetrans+4;
			memcpy(&buf[6],&DataLen,2);
			memcpy(&buf[8],&FileOffset,4);			

			//fseek(fp,FileOffset,SEEK_SET);	
			fseek(fp,FileOffset-upfile_info.read_cur_offset,SEEK_CUR);// you hua offset
			upfile_info.read_cur_offset = FileOffset;// you hua offset
			fread(&buf[12],filetrans,1,fp);
			upfile_info.read_cur_offset += filetrans;  // you hua offset
			
			chksum(buf,filetrans+12);			
			sendUdpMsg(sockfd,addr,buf,filetrans+13,0);				
			usleep(10000);			
		}
	}

	free(buf);
	buf = NULL;
} 

int CheckFile(int sockfd,const struct sockaddr_in *addr,int len)
{
	unsigned short DataLen;
	char *buf;//char buf[MAX_SIZE];	
	buf = (char *)malloc(MAX_SIZE);
	if(NULL == buf){
		return 0;//ToDo
	}


	buf[0] = 0x4C;
	buf[1] = 0x00;
	memcpy(&buf[2],(void *)&g_sys_info.DeviceID,4);
	memcpy(&buf[6],&DataLen,2);
	chksum(buf,8);
	
	sendUdpMsg(sockfd,addr,buf,9,MSGID_SERVER_TO_EDAS_RESP_CHECK_DATA);	
	free(buf);
	buf = NULL;

	return 1;
} 

int TransferDone(int sockfd,const struct sockaddr_in *addr,int len)
{
	unsigned short DataLen;
	//char buf[MAX_SIZE];
	char *buf;

	buf = (char *)malloc(MAX_SIZE);
	if(NULL == buf){
		return 0;//ToDo
	}

	buf[0] = 0x5C;
	buf[1] = 0x00;
	memcpy(&buf[2],(void *)&g_sys_info.DeviceID,4);
	memcpy(&buf[6],&DataLen,2);	
	chksum(buf,8);
	
	sendUdpMsg(sockfd,addr,buf,9,MSGID_SERVER_TO_EDAS_RESP_UPLOAD_CPLT);
	free(buf);
	buf = NULL;

	return 1;
} 



void doResponseConnect( MsgInfo *msg)
{	
	//is3gvalid = 1;	
}

void doRspUploadFileRes( MsgInfo *msg)
{
	if(upfile_info.fileTransState == REQUEST_UPFILE)
	{
		upfile_info.fileTransState = UPLOADING_FILE;	
		upfile_info.OffsetState = 0xFF;
		if(msg->ucBuff[8] == 0x01)
		{
			memcpy((void*)&upfile_info.OffsetList[0], (void*)&(msg->ucBuff[9]), 4);
			upfile_info.OffsetCnt = 1;
			upfile_info.OffsetState = 1;
			upfile_info.OffsetCur = upfile_info.OffsetList[0];

			UploadFile(upfile_info.fp_curTrans,net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
			upfile_info.fileTransState = CHECK_FILE;

			CheckFile(net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
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
}

void doRspCheckUploadData( MsgInfo *msg)
{
	int i;
	unsigned short rcvlen;
	
	if(upfile_info.fileTransState == CHECK_FILE)
	{			
		upfile_info.OffsetState = msg->ucBuff[8];
		memcpy(&rcvlen,&(msg->ucBuff[6]),2);

		switch(upfile_info.OffsetState)
		{		
			case 1:
			case 3:
				memcpy((void*)&upfile_info.OffsetCur,&(msg->ucBuff[9]),4);
				if(upfile_info.OffsetCur >= upfile_info.filesize)
				{
					//upfile_info.OffsetState = 4;//file end	
					upfile_info.fileTransState = TRANS_DONE;	
					TransferDone(net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));

					upfile_info.fileTransState = TRANS_STOP;
					//if(upfile_info.fp_curTrans_state == 1)
					{
						fflush(upfile_info.fp_curTrans);
						fclose(upfile_info.fp_curTrans);
						upfile_info.fp_curTrans_state = 0;
					}
					remove(upfile_info.curTransFileName);					
					saveUploadSuccessFileList(upfile_info.upsuccess_filename,upfile_info.filesize);
					
					filesRecord[upfile_info.curTransFileIndex].flag = 2;
					upfile_info.fileTransState = IDLE;
					save_recfile_list();
				}
				else
				{
					UploadFile(upfile_info.fp_curTrans,net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
					upfile_info.fileTransState = CHECK_FILE;	
					usleep(100000);
					CheckFile(net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
					usleep(100000);
				}
				break;
			case 2:
				upfile_info.OffsetCnt=(rcvlen-1)/4;
				
				for(i=0; i<upfile_info.OffsetCnt; i++)
				{
					memcpy((void*)&upfile_info.OffsetList[i],&(msg->ucBuff[9+4*i]),4);
					upfile_info.fileTransState = UPLOADING_FILE;	
				}	
				
				UploadFile(upfile_info.fp_curTrans,net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
				upfile_info.fileTransState = CHECK_FILE;
				usleep(100000);
				CheckFile(net3g.sockfd,&net3g.addr,sizeof(struct sockaddr_in));
				usleep(100000);
				break;	
			case 4:
				upfile_info.fileTransState = TRANS_STOP;
				if(upfile_info.fp_curTrans_state==1)
				{
					fflush(upfile_info.fp_curTrans);
					fclose(upfile_info.fp_curTrans);
					upfile_info.fp_curTrans_state = 0;
				}
				upfile_info.fileTransState = IDLE;
				break;
		}
	}
	
}
void doRspUploadDone( MsgInfo *msg)
{
	/*if(upfile_info.fileTransState == TRANS_DONE)
	{
		filesRecord[upfile_info.curTransFileIndex].flag = 2;
		//upfile_info.fileTransState = TRANS_STOP;	
		if(upfile_info.fp_curTrans_state == 1)
		{
			upfile_info.fp_curTrans_state = 0;
			fclose(upfile_info.fp_curTrans);
		}
		writeFilesRecords();
		upfile_info.fileTransState = IDLE;
		My_Printf(DEBUG_3G,"doRspUploadDone_dodone\n");
	}
	*/
}
void doRspReqDownloadCfg( MsgInfo *msg)
{
	char *buf;//char buf[MAX_SIZE];
	unsigned short DataLen = 5;	
	g_sys_info.downloading_cfg = TRUE;
	
	memcpy((void*)&cfgfile_info.filesize,&(msg->ucBuff[8+4]),4);
	
	cfgfile_info.block = (cfgfile_info.filesize/512);
	if(cfgfile_info.filesize%512) 
		cfgfile_info.block++;
	//20161008 xyl
	cfgfile_info.file_content = (char *)malloc(512*cfgfile_info.block);
	if(NULL == cfgfile_info.file_content)
	{
		cfgfile_info.state = 1;
		//ToDo
	}
	else
	{
		cfgfile_info.state = 0;
	}

	
	//printf("cfgfile_info.filesize:%d,cfgfile_info.block:%d\n",cfgfile_info.filesize,cfgfile_info.block);
	if(cfgfile_info.state == 0)
	{
		cfgfile_info.fp = fopen(EDASCFG_TMP,"wb");
		cfgfile_info.state = 1;
	}	
	
	buf = (char *)malloc(MAX_SIZE);
	if(NULL == buf){
		//ToDo
	}

	buf[0] = 0xE1;
	buf[1] = 0x00;
	memcpy(&buf[2],(void *)&g_sys_info.DeviceID,4);
	memcpy(&buf[6],&DataLen,2);	
	buf[8] = 0x01;
	memset(&buf[9],0,4);	
	chksum(buf,DataLen+8);
	
	sendUdpMsg(net3g.sockfd,&net3g.addr,buf,DataLen+9,0xE1);	
	free(buf);
	buf = NULL;
}


void doRspDownloadCfgFile( MsgInfo *msg)
{
	short datalen;
	int fileoffset;
	int blocknum;

	memcpy(&datalen,&(msg->ucBuff[6]),2);
	memcpy(&fileoffset,&(msg->ucBuff[8+4]),4);
	blocknum = (fileoffset/512);
	memcpy(&cfgfile_info.file_content[fileoffset],&(msg->ucBuff[12+4]),datalen-4-4);
	cfgfile_info.filecnt[blocknum] = 1;	
}
void doRspCheckCfgFile( MsgInfo *msg)
{
	char *buf;//char buf[MAX_SIZE];	
	unsigned short DataLen = 5;
	char respCode = 0x01;
	int i;

	for(i=0;i<cfgfile_info.block;i++)
	{
		if(cfgfile_info.filecnt[i] == 0)
			respCode = 0x03;
	}
		
	buf = (char *)malloc(MAX_SIZE);
	if(NULL == buf){
		//ToDo
	}
	buf[0] = 0xE3;
	buf[1] = 0x00;
	memcpy(&buf[2],(void *)&g_sys_info.DeviceID,4);
	memcpy(&buf[6],&DataLen,2);	
	buf[8] = respCode;
	memset(&buf[9],0,4);	
	chksum(buf,DataLen+8);
	
	sendUdpMsg(net3g.sockfd,&net3g.addr,buf,DataLen+9,0xE3);	
	sleep(5);
	if(respCode == 0x01)
	{
		//printf("doRspCheckCfgFile success,wait for reboot!\n");
		fwrite(cfgfile_info.file_content,cfgfile_info.filesize,1,cfgfile_info.fp);
		fclose(cfgfile_info.fp);
		sleep(1);
		remove("/media/sd-mmcblk0p1/EDAS_P_CFG.ers");
		sleep(1);
		system("mv /EDAS_P_CFG.ers /media/sd-mmcblk0p1/");		
		system("rm /media/sd-mmcblk0p1/record/*");
		sleep(1);
		
		//printf_va_args_en(DEBUG_SYS,"update_EDAS_P_CFG.ers_reboot!\n");
		sleep(1);
		system("reboot");
	}

	free(buf);
	buf = NULL;
}


void doRspReportCfgFileDone( MsgInfo *msg)
{
	cfgfile_info.state = 0;
	free(cfgfile_info.file_content);
	cfgfile_info.file_content = NULL;
}




/*
FILE *fp_curfile_t;
int uploadingfile(FILE *fp,int sockfd,const struct sockaddr_in *addr,int len)
{
	int res;
	if(Request_collect(sockfd,addr,len))
	{
		if(Request_upfile(fp,sockfd,addr,len))
		{
			while(OffsetState != 4)
			{			
				if((fp_curfile_t == fp)&&(g_sys_info.state_T15))
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
}*/












