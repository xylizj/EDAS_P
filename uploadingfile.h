#ifndef __UPLOADING_H_
#define __UPLOADING_H_

extern unsigned char rbuf[MAX_SIZE];

int uploadingfile(FILE *fp,int sockfd,const struct sockaddr_in *addr,int len);
#endif

