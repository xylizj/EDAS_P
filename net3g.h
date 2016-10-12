#ifndef __NET3G_H_
#define __NET3G_H_


struct _net3g{
	char *ServerIP;
	char *ServerPort;
	struct sockaddr_in addr;
	int sockfd;
	unsigned int rcvcnt;
	unsigned int rcvcnt_last;
};
extern struct _net3g net3g;






extern void init_3g_net(void);
extern void HeartBeatReport(int sockfd,const struct sockaddr_in *addr,int len);




#endif

