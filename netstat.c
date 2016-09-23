#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 
#include <sys/types.h>  
#include <sys/stat.h>  
#include <fcntl.h>

#include <signal.h> 
#include <sys/time.h> 

#define EDAS_SIGNUM 36

int edas_pid = 0;


int GetTtyACM0Stat( )
{
    char    buffer[BUFSIZ];
    FILE    *read_fp;
    int        chars_read;
    int        ret;
    
    memset( buffer, 0, BUFSIZ );
    read_fp = popen("ls /dev | grep ttyACM0", "r");
    if ( read_fp != NULL ) 
    {
        chars_read = fread(buffer, sizeof(char), BUFSIZ-1, read_fp);
        if (chars_read > 0) 
        {
            ret = 1;
        }
        else
        {
            ret = -1;
        }
        pclose(read_fp);
    }
    else
    {
        ret = -1;
    }

    return ret;
}


/**********************************************************************
* 函数名称： GetNetStat
* 功能描述： 检测网络链接是否断开
* 输入参数： 
* 输出参数： 无
* 返 回 值： 正常链接1,断开返回-1
* 其它说明： 本程序需要超级用户权限才能成功调用ifconfig命令
* 修改日期        版本号     修改人          修改内容
* ---------------------------------------------------------------------
* 2010/04/02      V1.0      eden_mgqw
***********************************************************************/ 
int GetNetStat( )
{
    char    buffer[BUFSIZ];
    FILE    *read_fp;
    int        chars_read;
    int        ret;
    
    memset( buffer, 0, BUFSIZ );
    read_fp = popen("ifconfig ppp0 | grep RUNNING", "r");
    if ( read_fp != NULL ) 
    {
        chars_read = fread(buffer, sizeof(char), BUFSIZ-1, read_fp);
        if (chars_read > 0) 
        {
            ret = 1;
        }
        else
        {
            ret = -1;
        }
        pclose(read_fp);
    }
    else
    {
        ret = -1;
    }
    return ret;
}


int laststat=0;
int GetEdasPid()
{
    char    buffer[BUFSIZ];
    FILE    *read_fp;
    int        chars_read;
    int        ret;
    
    memset( buffer, 0, BUFSIZ );
	read_fp = popen("ps | grep edas_p", "r");
    if ( read_fp != NULL ) 
    {
        chars_read = fread(buffer, sizeof(char), BUFSIZ-1, read_fp);
        if (chars_read > 0) 
        {
        	edas_pid = atoi(buffer);
            ret = 1;
        }
        else
        {
            ret = -1;
        }
        pclose(read_fp);
    }
    else
    {
        ret = -1;
    }
    return ret;
}

int main()
{	
	int curstat= -1;
	pid_t pid = 0;
	int signum;
	union sigval mysigval;
	int connectcnt = 0;

	signum = EDAS_SIGNUM;
	
	while(1)
	{		
		if(edas_pid == 0)
		{
			GetEdasPid();
			pid = edas_pid;
			GetEdasPid();
			if(pid == edas_pid)
			{
				
			}
			else
			{
				edas_pid = 0;
				pid = 0;
				sleep(2);
			}			
			continue;
		}
		if(1 == GetTtyACM0Stat())	
		{
			curstat = GetNetStat();
		}
		else
			curstat = -1;
		mysigval.sival_int=curstat;
		
		if(laststat != curstat) //net state change
		{ 
			if(edas_pid)
			if(sigqueue(pid,signum,mysigval)==-1)
			{
				printf_va_args("send error\n");
			}
			else
			{
				laststat = curstat;
			}	
		}
		if(1 != curstat )  //net broken
		{
			if(1 == GetTtyACM0Stat())
			{				
				if(fork()==0){
				if(execlp("pppd","pppd","call","wcdma",NULL)<0)
					perror("execl error");
					sleep(6);
					connectcnt++;
				}
			}
		}
		else
			connectcnt = 0;

		if(connectcnt > 3)
			sleep(10);
	}
    //
    return 0;
}
