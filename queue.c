#include <string.h>
#include <pthread.h>

#include "queue.h"
pthread_mutex_t g_queue_mutex; 

/* 构造一个空队列Q */
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
/* 销毁队列Q(无论空否均可) */
Status DestroyQueue(LinkQueue *Q)
{ 
	QueuePtr p = (*Q).front;
	QueuePtr q;

	while (p) /* 删除所有节点*/
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
/* 将Q清为空队列 */
Status ClearQueue(LinkQueue *Q)
{ 
	pthread_mutex_lock(&g_queue_mutex);

	QueuePtr p = (*Q).front->next; /* 头节点下一节点 */
	QueuePtr q;

	while (p) /* 删除头节点外所有节点*/
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
/* 若Q为空队列,则返回TRUE,否则返回FALSE */
Status QueueEmpty(LinkQueue Q)
{ 
 if (Q.rear == Q.front)
  return TRUE;
 else
  return FALSE;
}
/* 求队列的长度 */
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
/* 若队列不空,则用e返回Q的队头元素,并返回OK,否则返回ERROR */
Status GetHead(LinkQueue Q, QElemType *e) 
{ 
 if (Q.rear == Q.front)
  return ERROR;
 *e = Q.front->next->data;
 return OK;
}
/* 插入元素e为Q的新的队尾元素 */
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
/* 若队列不空,删除Q的队头元素,用e返回其值,并返回OK,否则返回ERROR */
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
	if((*Q).rear == p) /*只有一个节点 删除后哦置链表为空 */
	 (*Q).rear = (*Q).front;
	 
	free(p); /* 删除队首节点 */
	p = NULL;

		pthread_mutex_unlock(&g_queue_mutex);
	return OK;
}


/* 从队头到队尾依次对队列Q中每个元素调用函数vi()。一旦vi失败,则操作失败 */
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
