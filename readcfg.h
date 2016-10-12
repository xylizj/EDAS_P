#ifndef __READCFG_H_
#define __READCFG_H_


#include "kline.h"
#include "can.h"
#include "common.h"

#define EDASCFGFILE        "/media/sd-mmcblk0p1/EDAS_P_CFG.ers"


#define REQ_FREE_SRC        0x01
#define REQ_CONFIG_COM      0x02
#define REQ_START_DAQ       0x07
#define REQ_STOP_DAQ        0x08
#define REQ_CONFIG_SIGNAL   0x05

#define DOWN_CONFIG_CAN     0x03
#define DOWN_CONFIG_KLINE   0x04
#define DOWN_CONFIG_SIGNAL  0x06


#define SIGNAL_DIAG_CAN    		0x01
#define SIGNAL_DIAG_KLINE 	 	0x02
#define SIGNAL_EXTERNAL    		0x03
#define SIGNAL_SAE1939     		0x04
#define SIGNAL_GPS         		0x05
#define SIGNAL_AUTH_CODE	    0x06


typedef struct
{
  uint8_t	  Fmt;
  uint8_t	  BlockNum ;
  uint16_t   DataLength;
}tKWP_USB_HEAD;

typedef struct
{   
	uint32_t dwLocalID;
	uint8_t  ucRdDatSerID;
	uint8_t  ucLidLength;  //add
	uint8_t  ucMemSize;
	uint8_t  ucLogicChanIndex;	
	uint16_t wSampleCyc;
	uint8_t reserve1;
	uint8_t reserve2;
	uint32_t dwNextCommTime;    
	
}tCAN_SIGNAL;


typedef struct
{
    uint32_t dwTesterID;
    uint32_t dwEcuID;
    uint8_t  ucDiagType;
    uint8_t  ucLogicChanIndex;      //ucRdDatSerID -->ucLogicChanIndex
    uint8_t  reserve1;
    uint8_t  reserve2;
    tCAN_SIGNAL *pt_signal[50];
    uint8_t ucSignalNum;
    uint8_t channel;
}tCAN_LOGIC_CONFIG;

typedef struct
{
    uint8_t  bIsValid;
    uint8_t  nLogicDiagChanNum;
    uint16_t wBaudrate;
    uint32_t dwTesterID[CHAN_LOGIC_MAX];
    uint32_t dwEcuID[CHAN_LOGIC_MAX];
    tCAN_LOGIC_CONFIG *pt_logic_can[CHAN_LOGIC_MAX];
    uint8_t ucLogicChanNum;
}tCAN_COM_CONFIG;

typedef struct 
{
	uint32_t ID;
	uint8_t chan;
}tCAN1939;







typedef struct
{
	char Fmt;
	char BlockNum;
	short DataLen;
}tHEADER;

 typedef struct
{
	char Type;
	char Status;
	short BlockSize;
}tTYPE_HEADER;

typedef enum
{
	STATUS_IDLE,
	HEAD_RECEIVING,
	DATA_RECEIVING,
	CHKSUM_RECEIVING,
	STATUS_END
}READ_STATUS;

/*Global variable*/
extern tCAN_COM_CONFIG can_channel[CAN_CHANNEL_MAX];
extern tCAN_LOGIC_CONFIG can_logic[CAN_LOGIC_MAX];/*0~3:for CAN0;4-7:for CAN1*/
extern tCAN_SIGNAL can_signal[CAN_SIGNAL_MAX];
extern uint8_t can_logic_num;
extern int canfcnt[2];
extern struct can_filter canfilter[CAN_CHANNEL_MAX][100];

extern unsigned int  canid_diag_id[2][8];
extern unsigned int  canid_diag_cnt[2];


/*Global function*/
extern void read_edas_cfg(void);
extern void acquire_user_cfgset(void);
extern void updateAuthCode(void);


#endif



