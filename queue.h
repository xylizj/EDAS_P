#ifndef __QUEUE_H_
#define __QUEUE_H_
 
#include <stdio.h>
#include <stdlib.h>
 
#define TRUE 1
#define FALSE 0

#define OK 0
#define ERROR -1
#define OVERFLOW -2


typedef struct 
{
	unsigned char   ucServeId;       
	unsigned char   ucBuff[1024];           
	unsigned char   ucCheckSum;       //o��D��?��
	unsigned int 	len;
}MsgInfo;

typedef MsgInfo QElemType;
//typedef int QElemType;
typedef int Status; 
/* ���е���ʽ�洢�ṹ */
typedef struct QNode
{
 QElemType data;
 struct QNode *next;
}QNode,*QueuePtr;
typedef struct
{
 QueuePtr front; /* ��ͷָ�� */
 QueuePtr rear;  /* ��βָ�� */
}LinkQueue;
 
Status InitQueue(LinkQueue *Q);
Status DestroyQueue(LinkQueue *Q);
Status ClearQueue(LinkQueue *Q);
Status QueueEmpty(LinkQueue Q);
int QueueLength(LinkQueue Q);
Status GetHead(LinkQueue Q, QElemType *e);
Status EnQueue(LinkQueue *Q, QElemType *e);
Status DeQueue(LinkQueue *Q, QElemType *e);
Status QueueTraverse(LinkQueue Q, void(*vi)(QElemType));

#endif/*__QUEUE_H_*/
