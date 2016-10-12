#include <string.h>
#include <pthread.h>

#include "queue.h"
pthread_mutex_t g_queue_mutex; 

/* ����һ���ն���Q */
Status InitQueue(LinkQueue *Q)
{ 
	int ret;
	QueuePtr p = NULL;

	p = (QueuePtr)malloc(sizeof(QNode));
	if(NULL == p)
	{
		return ERROR;
	}
	p->next = NULL;
	(*Q).rear = p;
	(*Q).front = p; 
	ret=pthread_mutex_init(&g_queue_mutex, NULL);
	if(0 != ret)
	{
		return ERROR;
	}

	return OK;
}
/* ���ٶ���Q(���ۿշ����) */
Status DestroyQueue(LinkQueue *Q)
{ 
	QueuePtr p = (*Q).front;
	QueuePtr q;

	while (p) /* ɾ�����нڵ�*/
	{
		q = p;
		p = p->next; 
		free(q);
		q = NULL;
	}
	(*Q).rear = NULL;
	(*Q).front = NULL;

	pthread_mutex_destroy(&g_queue_mutex);

	return OK;
}
/* ��Q��Ϊ�ն��� */
Status ClearQueue(LinkQueue *Q)
{ 
	pthread_mutex_lock(&g_queue_mutex);

	QueuePtr p = (*Q).front->next; /* ͷ�ڵ���һ�ڵ� */
	QueuePtr q;

	while (p) /* ɾ��ͷ�ڵ������нڵ�*/
	{
		q = p;
		p = p->next; 
		free(q);
		q = NULL;
	 }
 
	(*Q).rear = (*Q).front;
	(*Q).front->next = NULL;
	pthread_mutex_unlock(&g_queue_mutex);

	return OK;
}
/* ��QΪ�ն���,�򷵻�TRUE,���򷵻�FALSE */
Status QueueEmpty(LinkQueue Q)
{ 
 if (Q.rear == Q.front)
  return TRUE;
 else
  return FALSE;
}
/* ����еĳ��� */
int QueueLength(LinkQueue Q)
{ 

	pthread_mutex_lock(&g_queue_mutex);
 int len = 0;
 QueuePtr p = Q.front;
 
 while (p != Q.rear)
 {
  p = p->next;
  len++;
 }
 
 	pthread_mutex_unlock(&g_queue_mutex);
 return len;
}
/* �����в���,����e����Q�Ķ�ͷԪ��,������OK,���򷵻�ERROR */
Status GetHead(LinkQueue Q, QElemType *e) 
{ 
 if (Q.rear == Q.front)
  return ERROR;
 *e = Q.front->next->data;
 return OK;
}
/* ����Ԫ��eΪQ���µĶ�βԪ�� */
Status EnQueue(LinkQueue *Q, QElemType *e)
{ 
	pthread_mutex_lock(&g_queue_mutex);
 QueuePtr p = (QueuePtr)malloc(sizeof(QNode));
 memcpy((char *)&p->data,(char *)e,sizeof(QElemType));
 //p->data = e;
 p->next = NULL;
 (*Q).rear->next = p;
 (*Q).rear = p;
 	pthread_mutex_unlock(&g_queue_mutex);
 return OK;
}
/* �����в���,ɾ��Q�Ķ�ͷԪ��,��e������ֵ,������OK,���򷵻�ERROR */
Status DeQueue(LinkQueue *Q, QElemType *e)
{ 
	pthread_mutex_lock(&g_queue_mutex);
	if ((*Q).front == (*Q).rear)
	{	
		pthread_mutex_unlock(&g_queue_mutex);
	return ERROR;
	}

	QueuePtr p = (*Q).front->next;
	memcpy((char *)e,(char *)&p->data,sizeof(QElemType));
	//*e = p->data;

	(*Q).front->next = p->next;
	if((*Q).rear == p) /*ֻ��һ���ڵ� ɾ����Ŷ������Ϊ�� */
	 (*Q).rear = (*Q).front;
	 
	free(p); /* ɾ�����׽ڵ� */
	p = NULL;

		pthread_mutex_unlock(&g_queue_mutex);
	return OK;
}


/* �Ӷ�ͷ����β���ζԶ���Q��ÿ��Ԫ�ص��ú���vi()��һ��viʧ��,�����ʧ�� */
Status QueueTraverse(LinkQueue Q, void(*vi)(QElemType))
{
	pthread_mutex_lock(&g_queue_mutex);
	QueuePtr p = Q.front->next;

	while (p != Q.rear)
	{
		vi(p->data);
		p = p->next; 
	}

	pthread_mutex_unlock(&g_queue_mutex);
	return OK;
}
