#ifndef __KLINE_H_
#define __KLINE_H_

#include "common.h"//for uint32 ...
#include "can.h"//for tRCV15765data

//#define B10400 0000014
#define B10400 0010017

/******KLine state************/   
#define K_BREAK         0
#define K_IDLE          1
#define K_INIT_DOWN     2
#define K_INIT_UP       3
#define K_INIT_DONE     4
#define K_SND_BYTE      5
#define K_SND_BYTE_RCV  6
#define K_SND_ALL       7
#define K_R_TIMEOUT     8
#define K_RCV_BYTE      9
#define K_RCV_ALL      10

#define KLINE_IO_INPUT    0U
#define KLINE_IO_OUTPUT   1U
   
#define KLINE_IO_HIGH     1U
#define KLINE_IO_LOW      0U
  
#define KLINE_STATUS_PWRONINIT      0U
#define KLINE_STATUS_FASTINIT_REQ   1U
#define KLINE_STATUS_FASTINIT       2U
#define KLINE_STATUS_WAKEUP         3U
#define KLINE_STATUS_STARTCOM       4U
#define KLINE_STATUS_WORKING        5U
   
#define KLINE_FORMAT_LENGTH_MASK  0x3FU
#define KLINE_FORMAT_ADDR_MASK    0xC0U
   
#define KLINE_HEADER_MODE_2       0x80U
#define KLINE_HEADER_MODE_3       0xC0U
	/* Initialize mode */
#define KLINE_FAST_INITIALIZE   0U
#define KLINE_5BAUD_INITIALIZE  1U
  
   /* Working mode */
#define KLINE_NORMAL_MODE      0U
#define KLINE_LISTENING_MODE   1U
  
   /* K line timing parameter */
   
   /* P1 inter byte time in ECU response */
#define KLINE_TIMING_P1_MAX    20U
   /* P4 inter byte time for tester request */
#define KLINE_TIMING_P4_MIN    2U
#define KLINE_TIMING_P4_MAX    200U
   /* Init timing */
#define KLINE_TIMING_INIT      25U
   /* Wakeup timing , must bigger than INIT */
#define KLINE_TIMING_WAKEUP    50U
  
   /* HW loop timer */
#define KLINE_HW_LOOP_TX     10000U
   
   /* Buffer size */
#define KLINE_TX_BUF_SIZE  260U
#define KLINE_RX_BUF_SIZE  268U


#define K_RCV15765_QUEUE_SIZE       (int)50
#define KBUF_SIZE 255


typedef enum
{
	K_INIT_BREAK = 0,
	K_INIT_ING1,
	K_INIT_ING2,
	K_INIT_OK,
	K_TIMEOUT
}KLINE_INIT_STATE;


typedef enum
{
	RX_UNINIT = 0,
	RX_IDLE,
	RX_HEADER_RECEIVING,
	RX_DATA_RECEIVING,
	RX_CHKSUM_RECEIVING,
	RX_END
}KLINE_RX_ENGINE_TYPE;
 
typedef enum
{
	TX_UNINIT = 0,
	TX_IDLE,
	TX_REQUEST
}KLINE_TX_ENGINE_TYPE;

typedef enum
{
	TX_WRONG_DATA = 0,
	TX_OK = 1
}KLINE_TX_RESULT_TYPE;

typedef struct
{
	KLINE_RX_ENGINE_TYPE engine;
	uint32_t timer;
	uint16_t dataLength;
	uint8_t  headerLength;
	uint16_t rxIndex;   
}KLINE_RX_STATE_TYPE;
 
typedef struct
{
	KLINE_TX_ENGINE_TYPE engine;
	uint32_t timer;
	uint16_t length;
	uint16_t txIndex;   
}KLINE_TX_STATE_TYPE;

typedef struct
{
	uint8_t      wp;                     /* write pointer */
	uint8_t      rp;                     /* read pointer */
	uint8_t      cnt;                    /* frame counter in queue */
	tRCV15765data  qdata[K_RCV15765_QUEUE_SIZE];   /* frame data */
}tK_RCV15765_QUEUE;


 typedef struct
 {
    uint8_t    bIsValid;
    uint8_t    ucDiagType;

    uint16_t   wBaudrate;
    uint8_t    Tester_InterByteTime_ms;
    uint8_t    ECU_MaxInterByteTime_ms;
    uint16_t   ECU_MaxResponseTime_ms;
    uint16_t   Tester_MaxNewRequestTime_ms;  

    uint8_t    nInitFrmLen;
    uint8_t    ucInitFrmData[10];
    uint8_t    ucLogicChanIndex;
    uint8_t    reserve1;
    uint8_t    reserve2;
}tKLINE_CONFIG;

typedef struct
{
    uint32_t dwLocalID;
	uint8_t  ucRdDatSerID;
	uint8_t ucLidLength;
	uint8_t ucMemSize;
	uint8_t  ucLogicChanIndex;
    uint16_t wSampleCyc;
    uint8_t reserve1;
    uint8_t reserve2;
	uint32_t dwNextTime;
}tKLINE_SIGNAL;


extern tKLINE_CONFIG kline_config[2];
extern tKLINE_SIGNAL *kline_siganl[50];//extern tKLINE_SIGNAL kline_siganl[50];
extern uint8_t k_sig_num;

extern KLINE_INIT_STATE KlineInitState[2];
extern KLINE_TX_STATE_TYPE kline_TxState;
extern KLINE_RX_STATE_TYPE kline_RxState;
extern tK_RCV15765_QUEUE k15765queue;


extern int OpenKline(void);
extern ssize_t read_kfile(int fd, unsigned char *buf, off_t offset, unsigned int to);
extern void handl_K_rcvdata(unsigned char *pbuf);





#endif

