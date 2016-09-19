#ifndef __CAN_H_
#define __CAN_H_

#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/socket.h>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/raw.h>

#ifndef AF_CAN
#define AF_CAN 29
#endif

#ifndef PF_CAN
#define PF_CAN AF_CAN
#endif

#define CHAN_LOGIC_MAX 4
#define CAN_CHANNEL_MAX 2
#define CAN_LOGIC_MAX 8
#define CAN_SIGNAL_MAX 100


#define kSingleFrame                0x00
#define kFirstFrame                 0x10
#define kConsecutiveFrame           0x20
#define kFlowControlFrame           0x30
#define kFlowStatus_Overrun         0x32
#define kErrorFrame                 0x40

#define kTxTimeout    50U	//50ms 
#define kRxTimeout    200U   //50ms
#define CAN_TX_QUEUE_MAX_INDEX  (CAN_TX_QUEUE_SIZE-1)

#define KWP_IDLE                 0x00
#define KWP_USB_CMD_RECEIVED     0x01
#define KWP_WAIT_ANSWER          0x02
#define KWP_ANSWER_RECEIVED      0x03
#define KLINE_ERR_BUSY           0x04

#define KWP_ERROR                0xFF

#define CAN_ERR_TX_OK               0x00U
#define CAN_ERR_RX_TIMEOUT          0x70U
//#define CAN_ERR_TX_TIMEOUT          0x80U
#define CAN_ERR_TX_BUSY             0x90U


typedef union uCANID
{
  uint32_t   l;
  uint16_t   w[2];
  uint8_t   b[4];
}tCANID;

typedef struct
{
  tCANID     id;               /*message id */
  uint8_t      data[8];          /*8 x data bytes */
  uint8_t      dl;               /*data length value */
  uint8_t      rsv;              /*not used */
  uint16_t       tsr;              /*time stamp register */
}tRXBUF;

typedef struct
{
  tCANID       id;               /* message id */
  uint8_t      data[8];          /* 8 x data bytes */
  uint8_t      dl;               /* DL(4 bit) */
}tMSG;

typedef enum
{
  TP_RX_IDLE = 0,
  TP_RX_CF,
  TP_RX_END,
  TP_RX_OVERLOAD
}TP_RX_ENGINE_TYPE;

typedef enum
{
  TP_TX_IDLE = 0,
  TP_TX_REQUEST,
  TP_TX_FC,             //2009-11-10, lzy, add for tx flow control frame
  TP_TX_WAIT_FC,
  TP_TX_CF,
  TP_TX_END
}TP_TX_ENGINE_TYPE;

#define CAN_RX_QUEUE_SIZE       (uint16_t)200

typedef struct
{
  uint16_t      wp;                     /* write pointer */
  uint16_t      rp;                     /* read pointer */
  uint16_t      cnt;                    /* frame counter in queue */
  tMSG      data[CAN_RX_QUEUE_SIZE];   /* frame data */
}tCAN_RX_QUEUE;

#define CAN_TX_QUEUE_SIZE       (uint8_t)64
typedef struct
{
  uint8_t      wp;                     /* write pointer */
  uint8_t      rp;                     /* read pointer */
  uint8_t      cnt;                    /* frame counter in queue */
  tMSG      data[CAN_TX_QUEUE_SIZE];   /* frame data */
}tCAN_TX_QUEUE;



#define CANBUFSIZE 100
typedef struct{
 	struct can_frame buf[CANBUFSIZE];
	char wp;
	char rp;
	char cnt;	
}tRAWCAN_RCV_QUEUE;

#define GPS_RCV_QUEUE_SIZE       (int)50
#define CAN_RCV1939_QUEUE_SIZE       (int)50
#define CAN_RCV15765_QUEUE_SIZE       (int)50

typedef struct
{
  uint16_t     len;               /* message id */
  uint8_t      data[24];          /* 8 x data bytes */
}tRCV1939data;
typedef struct
{
  uint16_t     len;               /* message id */
  uint8_t      data[300];          /* 8 x data bytes */
}tRCV15765data;

typedef struct
{
  uint16_t     len;               /* message id */
  uint8_t      data[50];          /* 8 x data bytes */
}tRcvGpsData;


typedef struct
{
  uint8_t      wp;                     /* write pointer */
  uint8_t      rp;                     /* read pointer */
  uint8_t      cnt;                    /* frame counter in queue */
  tRcvGpsData      qdata[GPS_RCV_QUEUE_SIZE];   /* frame data */
}tGPS_RCV_QUEUE;


typedef struct
{
  uint8_t      wp;                     /* write pointer */
  uint8_t      rp;                     /* read pointer */
  uint8_t      cnt;                    /* frame counter in queue */
  tRCV1939data      qdata[CAN_RCV1939_QUEUE_SIZE];   /* frame data */
}tCAN_RCV1939_QUEUE;

typedef struct
{
  uint8_t      wp;                     /* write pointer */
  uint8_t      rp;                     /* read pointer */
  uint8_t      cnt;                    /* frame counter in queue */
  tRCV15765data      qdata[CAN_RCV15765_QUEUE_SIZE];   /* frame data */
}tCAN_RCV15765_QUEUE;


extern int s_can0,s_can1;
extern tCAN_RCV1939_QUEUE can1939buf[2];

extern TP_RX_ENGINE_TYPE TpRxState[CAN_LOGIC_MAX];
extern TP_TX_ENGINE_TYPE TpTxState[CAN_LOGIC_MAX]; 

extern volatile tCAN_RX_QUEUE inq[CAN_LOGIC_MAX];
extern volatile tCAN_TX_QUEUE outq[CAN_LOGIC_MAX];
extern uint16_t uiTxDataLength[CAN_LOGIC_MAX];

extern uint16_t    uiRxIndex[CAN_LOGIC_MAX];
extern uint8_t     ucRxSn[CAN_LOGIC_MAX];
extern uint16_t    uiRxReminder[CAN_LOGIC_MAX];
extern uint16_t    uiRxTimer[CAN_LOGIC_MAX];

extern uint16_t    uiTxIndex[CAN_LOGIC_MAX];
extern uint8_t     ucTxSn[CAN_LOGIC_MAX];
extern uint16_t    uiTxReminder[CAN_LOGIC_MAX];
extern uint8_t     ucTxTimer[CAN_LOGIC_MAX];

extern uint8_t     ucTSmin[CAN_LOGIC_MAX];
extern uint8_t     ucBSmax[CAN_LOGIC_MAX];
extern uint16_t    fc_count[CAN_LOGIC_MAX];
extern uint8_t         USB_Flash_Status[CAN_LOGIC_MAX];
extern uint16_t uiRxDataLength[CAN_LOGIC_MAX];

extern uint8_t  ucRxBuff[CAN_LOGIC_MAX][268];

extern int ulFlashRxId[3],ulFlashTxId[3];
extern volatile uint32_t currentlid;
extern volatile uint32_t currentMSize;
extern int currenLogicChan;
extern uint8_t currentLidlen;
extern uint8_t b15765rcv;
extern uint8_t I2L[CAN_LOGIC_MAX]; /*indexnum to logic number*/   /*查找I对应具体的逻辑通道值*/
extern tCAN_RCV15765_QUEUE can15765buf;
extern tGPS_RCV_QUEUE bps_buf;



void task_can0_read();
void task_can1_read();
uint8_t Rx_ISO15765_Frame(struct can_frame *fr,uint8_t channel);


#endif

