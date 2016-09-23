#ifndef __COMMON_H_
#define __COMMON_H_


#include <stdbool.h>
#include <stdarg.h>
#include "queue.h"

#define FileDeviceName "/devicename.txt"


#define CFG_3G_ENABLE 1
#define CFG_GPS_ENABLE 1



#define EDAS_FILE "/media/sd-mmcblk0p1/20141217.ers"
#define FileName "20141217.ers"
#define FilePath "/media/sd-mmcblk0p1/20141217.ers"

#define RECORDPATH "/media/sd-mmcblk0p1/record/rec_"
#define RECORDDIR "/media/sd-mmcblk0p1/record/"

#define UPLOADEDFILELIST "/media/sd-mmcblk0p1/uploadSuccessFileList.txt"

#define MAX_SIZE 1024
#define DOWN_CONFIG_SIGNAL  0x06
#define UP_DAQ_SIGNAL       0xFD
#define USB_QUEUE_SIZE 2500


#define DUG_PRINTF_EN 1
#define DUG_1939 	1
#define DUG_KLINE 	1



#define  dug_readcfg	1
#define  dug_gps		1
#define  dug_can		1
#define  dug_k			1
#define  dug_3g			1
#define  dug_rtc		1
#define  dug_sd			1
#define  dug_sys		1

#define EDASCFG_TMP        "/EDAS_P_CFG.ers"

//=================================== MsgID ===========================================================

//EDAS端报文列表
#define MSGID_EDAS_TO_SERVER_CONNECT          0x1C	  //EDAS请求连接到服务器
#define MSGID_EDAS_TO_SERVER_UPLOAD_FILE      0x2C    //EDAS向服务器请求上传文件 	
#define MSGID_EDAS_TO_SERVER_UPLOAD_DATA      0x3C    //EDAS-P向服务器上传文件内容
#define MSGID_EDAS_TO_SERVER_CHECK_DATA       0x4C    //EDAS向服务器确认数据是否成功上传	
#define MSGID_EDAS_TO_SERVER_UPLOAD_CPLT      0x5C    //EDAS向服务器报告文件传输完毕
#define MSGID_EDAS_TO_SERVER_HEART            0x6C    //EDAS发送到服务器的心跳报文

#define MSGID_SERVER_TO_EDAS_RESP_CONNECT          0xC1    //服务器响应EDAS的连接请求
#define MSGID_SERVER_TO_EDAS_RESP_UPLOAD_FILE      0xC2    //服务器响应EDAS 上传文件请求
#define MSGID_SERVER_TO_EDAS_RESP_UPLOAD_DATA      0xC3    //保留
#define MSGID_SERVER_TO_EDAS_RESP_CHECK_DATA       0xC4    //服务器响应数据正确性请求
#define MSGID_SERVER_TO_EDAS_RESP_UPLOAD_CPLT      0xC5    //服务器响应文件传输完毕
#define MSGID_SERVER_TO_EDAS_RESP_HEART            0xC6    //保留

#define MSGID_SERVER_TO_EDAS_REQ_DOWNLOAD_CFG      0x1E
#define MSGID_SERVER_TO_EDAS_DOWNLOAD_CFGFILE      0x2E
#define MSGID_SERVER_TO_EDAS_CHECK_DOWNLOADFILE    0x3E
#define MSGID_SERVER_TO_EDAS_REPORT_DOWNLOADDONE   0x4E



//PC端报文列表
#define MSGID_PC_TO_SERVER_CONNECT          0x1D    //PC请求连接到服务器
#define MSGID_PC_TO_SERVER_QUERY_ONLINE     0x2D    //PC请求查询EDAS是否在线
#define MSGID_PC_TO_SERVER_OPEN_MNT         0x3D    //PC请求开启监控模式
#define MSGID_PC_TO_SERVER_EXIT_MNT         0x4D    //PC请求退出监控模式
#define MSGID_PC_TO_SERVER_HEART            0x6D    //PC发送到服务器的心跳报文


#define MSGID_SERVER_TO_PC_RESP_CONNECT          0xD1    //服务器响应PC端的连接请求	
#define MSGID_SERVER_TO_PC_RESP_QUERY_ONLINE     0xD2    //服务器响应PC查询请求	
#define MSGID_SERVER_TO_PC_RESP_OPEN_MNT         0xD3    //服务器响应开启监控模式请求	
#define MSGID_SERVER_TO_PC_RESP_EXIT_MNT         0xD4    //服务器响应退出监控模式请求	
#define MSGID_SERVER_TO_PC_RESP_HEART            0xD6    //保留	

#define MsgState_snd 0x00
#define MsgState_rcv 0x01

typedef enum
{
  IDLE = 0,
  REQUEST_UPFILE,
  UPLOADING_FILE,
  CHECK_FILE,
  TRANS_DONE,
  TRANS_STOP
}File_Trans_State;


/*tEDAS_P_STATE  0:stop work, 1:working nomally, 2:not configuration*/
typedef struct
{
	//char state_cfg;
	char state_T15;
	char state_3g;
	volatile int pppd_pid;
	char net_stat;
	char state_gps;
	char state_k;
	char state_can0_1939;
	char state_can1_1939;
	char state_can0_15765;
	char state_can1_15765;	
}tEDAS_P_STATE;

typedef unsigned char 		uint8_t;
typedef unsigned short int       uint16_t;
typedef unsigned int 		uint32_t;
typedef unsigned int    	BOOL;
typedef unsigned char 		BYTE;
typedef unsigned char 		byte;

typedef struct
{
	unsigned char filename[255];
	//int filecnt;
	int offset;
	int flag;       // 0:no trans 1:transce half 2:trans done	
}tFilesRecord;


typedef struct
{
	unsigned char msgid;
	unsigned char state;
	unsigned char buf[1024];
	int reSndCnt;
	int len;
}tMsgState;




extern volatile double latitude;
extern volatile double longitude;    
extern volatile double speed;
extern volatile double height;


extern volatile unsigned int US_SECOND;
extern volatile unsigned int US_MILISECOND;



extern int fd_k;
extern int sockfd;

extern tFilesRecord  filesRecord[100];
extern int filescnt;

extern tFilesRecord  filesOldRecord[100];
extern int fileoldscnt;

extern char cur3gfilename[100];
extern int cur3gfilename_len;
extern char CAR_ID[40];
extern char T15_state;

extern int detect3g;
extern int is3gvalid;
extern int is3gsocket;
extern LinkQueue msg_queue;
extern tMsgState curMsgstate;
extern int msgRxTimeoutCnt;
extern File_Trans_State fileTransState;
extern int curTransFile;
extern char curTransFileName[255];
extern FILE *fp_curTrans;
extern int isSaveGps;
extern unsigned int DeviceID;
extern int g_fp_curTrans_state;

extern unsigned int g_SysCnt;
extern unsigned int g_Rcv3gCnt;

extern unsigned int g_LastRcvCnt;
extern unsigned int g_3gState;
//extern volatile int g_pppd_pid;
extern char curnetstat;
extern char g_ServerIP[20];
extern char g_ServerPort[10];


extern bool g_isDownloadCfg;
extern volatile tEDAS_P_STATE g_sys_info;
extern volatile unsigned int g_read_cur_offset;
extern char g_rtc_state;

extern pthread_mutex_t g_file_mutex;
extern pthread_mutex_t g_rtc_mutex;

extern void sendUdpMsg(char *buf,int len,unsigned char rxMsgId);

extern unsigned int getSystemTime(void);
extern void chksum(char *buf,int len);
extern void upDateTime(void);

extern void creat_FileName(unsigned char *ptPath,unsigned char *name);

extern void writeFilesRecords(void);
extern char *itoa(int num, char *str, int radix);
extern void reinit_3g_net(void);
extern void makeKillCommand(char *command,int pid);
extern void task_can(void);
extern void task_k(void);
extern void task_ChkSndFile(void);
extern void task_sd(void);
extern void task_gps(void);
extern void task_udprcv(void);
extern void task_handle_msg(void);
extern void task_check_msgrx(void);
extern void task_heartbeat(void);
extern void getFilesName(void);
extern void writeFilesRecords(void);
extern void printf_va_args(const char* fmt, ...);

extern int getSDstatus(unsigned int *total,unsigned int *used,unsigned int *free);
extern void init_edas_state(void);
extern void init_user_mutex(void);
extern void utc2cst(unsigned char *utc,unsigned char *cst);
extern int set_rtctime(unsigned char *cst);

#endif

