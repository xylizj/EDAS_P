/*********************Copyright(c)************************************************************************
**          Guangzhou ZHIYUAN electronics Co.,LTD
**
**              http://www.embedtools.com
**
**-------File Info---------------------------------------------------------------------------------------
** File Name:               serial-test.c
** Latest modified Data:    2008-05-19
** Latest Version:          v1.1
** Description:             NONE
**
**--------------------------------------------------------------------------------------------------------
** Create By:               zhuguojun
** Create date:             2008-05-19
** Version:                 v1.1
** Descriptions:            epc-8000's long time test for serial 1,2,3,4
**
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/
#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>  
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <termios.h>
#include <errno.h>   
#include <limits.h> 
#include <asm/ioctls.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include "common.h"


#define DATA_LEN                0xFF                                    /* test data's len              */

#define  B9600	0000015

//#define DEBUG                 1


/*********************************************************************************************************
** Function name:           openSerial
** Descriptions:            open serial port at raw mod
** input paramters:         iNum        serial port which can be value at: 1, 2, 3, 4
** output paramters:        NONE
** Return value:            file descriptor
** Create by:               zhuguojun
** Create Data:             2008-05-19
**--------------------------------------------------------------------------------------------------------
** Modified by:
** Modified date:
**--------------------------------------------------------------------------------------------------------
*********************************************************************************************************/
static int openSerial(char *cSerialName)
{
    int iFd;

    struct termios opt; 	

    iFd = open(cSerialName,  O_RDWR | O_NOCTTY | O_NDELAY);                        
    if(iFd < 0) {
        perror(cSerialName);
        return -1;
    }

    tcgetattr(iFd, &opt);      

     cfsetispeed(&opt, B9600);
     cfsetospeed(&opt, B9600);

	#if 1
    /*
     * raw mode
     */
    opt.c_lflag   &=   ~(ECHO   |   ICANON   |   IEXTEN   |   ISIG);
    opt.c_iflag   &=   ~(BRKINT   |   ICRNL   |   INPCK   |   ISTRIP   |   IXON);
    opt.c_oflag   &=   ~(OPOST);
    opt.c_cflag   &=   ~(CSIZE   |   PARENB);
    opt.c_cflag   |=   CS8;

    /*
     * 'DATA_LEN' bytes can be read by serial
     */
    opt.c_cc[VMIN]   =   DATA_LEN;                                      
    opt.c_cc[VTIME]  =   150;
	#endif

	///////////////////////////////////////
	#if 0

	opt.c_lflag   &=   ~(ICANON | ECHO | ECHOE | ISIG);
	opt.c_iflag   &=   ~(BRKINT   |   ICRNL   |   INPCK   |   ISTRIP   |   IXON);
	//                        
	opt.c_oflag   &=	~(OPOST);	 opt.c_cflag   &=	~(CSIZE   |   PARENB);
	opt.c_cflag &= ~CSIZE;
	opt.c_cflag	|=	 CS8;	 /* 	* 'DATA_LEN' bytes can be read by serial	 */    
	//opt.c_cc[VMIN]	=	DATA_LEN;										   
	//opt.c_cc[VTIME]	=	150;	
	opt.c_cc[VMIN]	=	0;										   
	opt.c_cc[VTIME]	=	0;
	#endif

    if (tcsetattr(iFd,   TCSANOW,   &opt)<0) {
        return   -1;
    }


    return iFd;
}

unsigned char gps_index;
unsigned char gps_buff[80];
unsigned char commaIndex = 0;
unsigned char commaLocation[20]; 

char gps_latitude[15];
char gps_longitude[15];    
char gps_speed[15];
char gps_height[15]; 

char gps_latitude_du[15];
char gps_longitude_du[15]; 

char gps_latitude_fen[15];
char gps_longitude_fen[15]; 

char gps_utctime[15];
char gps_utcdate[15]; 

char gpsflg = 0; //0x01:latitude;0x02:longitude;0x04:speed;0x08:height;

char g_rtc_state = 0; //0x00: wait update rtc; 0x01: update done

void task_gps()
{
	char tmp[1024];
	int len;
	int fd, i;
	char   flagGA;
	unsigned char utc[6];
	unsigned char localtime[6];

	fd = openSerial("/dev/ttySP1");
	
	while (1) {
	  if(gpsflg != 0x0F)
	  {
		len = read(fd, tmp, 1024);	
		usleep(1000);
		for(i = 0; i < len; i++)
		{			
			switch(tmp[i])
			{
				case '$':
					gps_index = 1;
					gps_buff[0] = '$';
					commaIndex = 0;
					break;
				default:             
                	if( gps_index < 80 )
                	{
                		gps_buff[gps_index] = tmp[i];
                    	gps_index++;
						if(tmp[i] == ',')
	                    {
							if(commaIndex < 20)
							{
	                        	commaLocation[commaIndex] = gps_index-1;
	                        	commaIndex++;
							}
	                    } 
						if(tmp[i]== '\n')
						{
							if(('M' == gps_buff[4]) && ('C' == gps_buff[5]))
							{
	                            if('A'==gps_buff[commaLocation[1]+1])
	                            {
	                            	if(g_rtc_state == 0)
	                            	{	                            		
										memset(gps_utctime,0,15);	                                    
	                                    memcpy(&gps_utctime[0],&gps_buff[commaLocation[0]+1],commaLocation[1]-commaLocation[0]-1);//utc_time
	                                    memset(gps_utcdate,0,15);	                                    
	                                    memcpy(&gps_utcdate[0],&gps_buff[commaLocation[8]+1],commaLocation[9]-commaLocation[8]-1);//utc_time
										utc[0] = 10*(gps_utcdate[4]-'0')+(gps_utcdate[5]-'0');//year
										utc[1] = 10*(gps_utcdate[2]-'0')+(gps_utcdate[3]-'0');//mouth
										utc[2] = 10*(gps_utcdate[0]-'0')+(gps_utcdate[1]-'0');//date
										utc[3] = 10*(gps_utctime[0]-'0')+(gps_utctime[1]-'0');//hour
										utc[4] = 10*(gps_utctime[2]-'0')+(gps_utctime[3]-'0');//minute
										utc[5] = 10*(gps_utctime[4]-'0')+(gps_utctime[5]-'0');//second
										//printf("gps_utctime : %s\n",gps_utctime);
										//printf("gps_utcdate : %s\n",gps_utcdate);
										utc2cst(utc,localtime);										
										set_rtctime(localtime);
										
	                            		g_rtc_state = 1;
	                            	}
	                                if(0x00 == (gpsflg & 0x01))
	                                {
	                                    memset(gps_latitude,0,15);	                                    
	                                    memcpy(&gps_latitude[0],&gps_buff[commaLocation[2]+1],commaLocation[3]-commaLocation[2]-1);//latitude            
	                                }
	                                if(0x00 == (gpsflg & 0x02))
	                                {
	                                    memset(gps_longitude,0,15);	                                    
	                                    memcpy(&gps_longitude[0],&gps_buff[commaLocation[4]+1],commaLocation[5]-commaLocation[4]-1);//longitude            
	                                }
	                                if(0x00 == (gpsflg & 0x04))
	                                {
	                                    memset(gps_speed,0,15);	                                    
	                                    memcpy(&gps_speed[0],&gps_buff[commaLocation[6]+1],commaLocation[7]-commaLocation[6]-1);//speed
	                                }
	                                gpsflg = (gpsflg|0x07);
	                            }
	                        }
							if(('G' == gps_buff[4]) && ('A' == gps_buff[5]))
							{
	                            flagGA = gps_buff[commaLocation[5]+1];
	                            switch(flagGA)
	                            {
	                                case '1':
	                                case '2':
	                                case '4':
	                                    if(0x00 == (gpsflg & 0x08))
	                                    {
	                                        memset(gps_height,0,15); 	                                        
	                                        memcpy(&gps_height[0],&gps_buff[commaLocation[8]+1],commaLocation[9]-commaLocation[8]-1);//height
	                                    }
	                                    if(0x00 == (gpsflg & 0x01))
	                                    {	                                        
	                                        memcpy(&gps_latitude[0],&gps_buff[commaLocation[1]+1],commaLocation[2]-commaLocation[1]-1);//latitude            
	                                    }
	                                    if(0x00 == (gpsflg & 0x02))
	                                    {	                                        
	                                        memcpy(&gps_longitude[0],&gps_buff[commaLocation[3]+1],commaLocation[4]-commaLocation[3]-1);//longitude     
	                                    }
	                                    gpsflg = (gpsflg|0x0B);
	                                    break;
	                                default:
	                                    break;
	                            }
	                        }
							if(('T' == gps_buff[4]) && ('G' == gps_buff[5]))
							{
							 	if(0x00 == (gpsflg & 0x04))
		                        {
		                            memset(gps_speed,0,15);		                            
		                            memcpy(&gps_speed[0],&gps_buff[commaLocation[4]+1],commaLocation[5]-commaLocation[4]-1);//speed
									gpsflg = gpsflg & 0x04;
								}
							}							
						}
                	}
										
					break;
			}

			if(0x0F == gpsflg)
			{
				break;
			}
		}
	   }

		if(0x0F == gpsflg)
		{
			My_Printf(dug_gps,"s_latitude  = %s\n",gps_latitude);
			My_Printf(dug_gps,"s_longitude = %s\n",gps_longitude);
			My_Printf(dug_gps,"s_speed	  = %s\n",gps_speed);
			My_Printf(dug_gps,"s_height	  = %s\n",gps_height);

			memcpy(gps_latitude_du,gps_latitude,2);
			memcpy(gps_latitude_fen,&gps_latitude[2],7);

			memcpy(gps_longitude_du,gps_longitude,3);
			memcpy(gps_longitude_fen,&gps_longitude[3],7);

			latitude  = (atof(gps_latitude_fen)/60)+atoi(gps_latitude_du);
			longitude = (atof(gps_longitude_fen)/60)+atoi(gps_longitude_du);		
			speed     = 1.852 *atof(gps_speed);
			height    = atof(gps_height);

			gpsflg = 0;	
			if(g_edas_state.state_gps != 2)
				g_edas_state.state_gps = 1;
		}
	}
}


/*********************************************************************************************************
    end file
*********************************************************************************************************/
