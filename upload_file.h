#ifndef __UPLOADING_H_
#define __UPLOADING_H_

#define CFGFILE_MAX_NUM 10
#define CFGFILE_CONTENT_LEN (512*CFGFILE_MAX_NUM)

typedef enum
{
  IDLE = 0,
  REQUEST_UPFILE,
  UPLOADING_FILE,
  CHECK_FILE,
  TRANS_DONE,
  TRANS_STOP
}File_Trans_State;

struct _download_cfgfile_info{
	volatile int filesize;
	FILE * fp;
	char state;
	volatile int block;
	int filecnt[CFGFILE_MAX_NUM];
	char *file_content;//char file_content[CFGFILE_CONTENT_LEN];	
};

struct _upload_file_info{
	volatile int filesize;
	off_t read_cur_offset;
	volatile unsigned int OffsetList[20];
	volatile unsigned int OffsetCnt;
	volatile unsigned int OffsetState; //0x01: to be translate;0x02:lost offset;0x03:all lost;0x04:file end
	volatile unsigned int OffsetCur;
	int curTransFileIndex;
	char curTransFileName[100];
	FILE *fp_curTrans;	
	char fp_curTrans_state;
	File_Trans_State fileTransState;
	char upsuccess_filename[100];
	int upsuccess_filename_len;
};



extern struct _upload_file_info upfile_info;










extern void sendUdpMsg(int sockfd,const struct sockaddr_in *addr,char *buf,int len,unsigned char rxMsgId);
extern void doResponseConnect( MsgInfo *msg);
extern void doRspUploadFileRes( MsgInfo *msg);
extern void doRspCheckUploadData( MsgInfo *msg);
extern int Request_upfile(FILE *fp,int sockfd,const struct sockaddr_in *addr,int len);
extern void doRspUploadDone( MsgInfo *msg);
extern void doRspReqDownloadCfg( MsgInfo *msg);
extern void doRspDownloadCfgFile( MsgInfo *msg);
extern void doRspCheckCfgFile( MsgInfo *msg);
extern void doRspReportCfgFileDone( MsgInfo *msg);

extern int Request_collect(int sockfd,const struct sockaddr_in *addr,int len);



#endif/*__UPLOADING_H_*/

