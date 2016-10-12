#include <fcntl.h> 
#include <termios.h>
//#include <errno.h>   
#include <string.h>
#include "common.h"
#include "gps.h"
#include "readcfg.h"


struct _gps_c_struct_ gps_c_struct;
struct _gps_tmp_struct_ gps_tmp_struct;
struct _gps_phy_struct_ gps_phy_struct;



int openSerial(char *cSerialName)
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

    if (tcsetattr(iFd,   TCSANOW,   &opt)<0) 
    {
        return   -1;
    }

    return iFd;
}




static
void settime_gps2rtc(void)
{
	unsigned char utc[6];
	unsigned char localtime[6];
	
	memset(gps_tmp_struct.tmp_utctime,0,15);										
	memcpy(&gps_tmp_struct.tmp_utctime[0],&gps_c_struct.c_buff[gps_c_struct.comma_location[0]+1],gps_c_struct.comma_location[1]-gps_c_struct.comma_location[0]-1);//utc_time
	memset(gps_tmp_struct.tmp_utcdate,0,15);										
	memcpy(&gps_tmp_struct.tmp_utcdate[0],&gps_c_struct.c_buff[gps_c_struct.comma_location[8]+1],gps_c_struct.comma_location[9]-gps_c_struct.comma_location[8]-1);//utc_time
	utc[0] = 10*(gps_tmp_struct.tmp_utcdate[4]-'0')+(gps_tmp_struct.tmp_utcdate[5]-'0');//year
	utc[1] = 10*(gps_tmp_struct.tmp_utcdate[2]-'0')+(gps_tmp_struct.tmp_utcdate[3]-'0');//mouth
	utc[2] = 10*(gps_tmp_struct.tmp_utcdate[0]-'0')+(gps_tmp_struct.tmp_utcdate[1]-'0');//date
	utc[3] = 10*(gps_tmp_struct.tmp_utctime[0]-'0')+(gps_tmp_struct.tmp_utctime[1]-'0');//hour
	utc[4] = 10*(gps_tmp_struct.tmp_utctime[2]-'0')+(gps_tmp_struct.tmp_utctime[3]-'0');//minute
	utc[5] = 10*(gps_tmp_struct.tmp_utctime[4]-'0')+(gps_tmp_struct.tmp_utctime[5]-'0');//second
	utc2cst(utc,localtime); 									
	set_rtctime(localtime); 
}


static
void c2tmp_MCA(char gpsflg)
{
	if(0x00 == (gpsflg & GPSFLG_LAT))
	{
		memset(gps_tmp_struct.tmp_latitude,0,15);										
		memcpy(&gps_tmp_struct.tmp_latitude[0],&gps_c_struct.c_buff[gps_c_struct.comma_location[2]+1],gps_c_struct.comma_location[3]-gps_c_struct.comma_location[2]-1);//latitude			  
	}
	if(0x00 == (gpsflg & GPSFLG_LNG))
	{
		memset(gps_tmp_struct.tmp_longitude,0,15);									
		memcpy(&gps_tmp_struct.tmp_longitude[0],&gps_c_struct.c_buff[gps_c_struct.comma_location[4]+1],gps_c_struct.comma_location[5]-gps_c_struct.comma_location[4]-1);//longitude 		
	}
	if(0x00 == (gpsflg & GPSFLG_SPD))
	{
		memset(gps_tmp_struct.tmp_speed,0,15);									
		memcpy(&gps_tmp_struct.tmp_speed[0],&gps_c_struct.c_buff[gps_c_struct.comma_location[6]+1],gps_c_struct.comma_location[7]-gps_c_struct.comma_location[6]-1);//speed
	}
}

static
void c2tmp_GA(char gpsflg)
{
	if(0x00 == (gpsflg & GPSFLG_HIT))
	{
		memset(gps_tmp_struct.tmp_height,0,15); 										
		memcpy(&gps_tmp_struct.tmp_height[0],&gps_c_struct.c_buff[gps_c_struct.comma_location[8]+1],gps_c_struct.comma_location[9]-gps_c_struct.comma_location[8]-1);//height
	}
	if(0x00 == (gpsflg & GPSFLG_LAT))
	{											
		memcpy(&gps_tmp_struct.tmp_latitude[0],&gps_c_struct.c_buff[gps_c_struct.comma_location[1]+1],gps_c_struct.comma_location[2]-gps_c_struct.comma_location[1]-1);//latitude			  
	}
	if(0x00 == (gpsflg & GPSFLG_LNG))
	{											
		memcpy(&gps_tmp_struct.tmp_longitude[0],&gps_c_struct.c_buff[gps_c_struct.comma_location[3]+1],gps_c_struct.comma_location[4]-gps_c_struct.comma_location[3]-1);//longitude  
	}
}



bool process_gps_ser_buf(char *gps_ser_raw_buf, int len)
{
	int i;
	static char gpsflg = 0;		
	
	for(i=0; i<len; i++)
	{			
		if('$' == gps_ser_raw_buf[i])
		{
			gps_c_struct.c_index = 1;
			gps_c_struct.c_buff[0] = '$';
			gps_c_struct.comma_idx = 0;
		}
		else
		{
			if( gps_c_struct.c_index < GPS_C_BUF_LEN )
			{
				gps_c_struct.c_buff[gps_c_struct.c_index] = gps_ser_raw_buf[i];
				gps_c_struct.c_index++;
				if(gps_ser_raw_buf[i] == ',')
				{
					if(gps_c_struct.comma_idx < 20)
					{
						gps_c_struct.comma_location[gps_c_struct.comma_idx] = gps_c_struct.c_index-1;
						gps_c_struct.comma_idx++;
					}
				} 
				if(gps_ser_raw_buf[i]== '\n')
				{
					if(('M' == gps_c_struct.c_buff[4]) && ('C' == gps_c_struct.c_buff[5]))
					{
						if('A'==gps_c_struct.c_buff[gps_c_struct.comma_location[1]+1])
						{
							if(g_sys_info.state_rtc == 0)
							{										
								settime_gps2rtc();
								g_sys_info.state_rtc = 1;
							}
							c2tmp_MCA(gpsflg);
							gpsflg = (gpsflg|0x07);
						}
					}
					if(('G' == gps_c_struct.c_buff[4]) && ('A' == gps_c_struct.c_buff[5]))
					{
						char flagGA;
						flagGA = gps_c_struct.c_buff[gps_c_struct.comma_location[5]+1];
						switch(flagGA)
						{
							case '1':
							case '2':
							case '4':							
								c2tmp_GA(gpsflg);
								gpsflg = (gpsflg|0x0B);
								break;
							default:
								break;
						}
					}
					if(('T' == gps_c_struct.c_buff[4]) && ('G' == gps_c_struct.c_buff[5]))
					{
						if(0x00 == (gpsflg & GPSFLG_SPD))
						{
							memset(gps_tmp_struct.tmp_speed,0,15); 								
							memcpy(&gps_tmp_struct.tmp_speed[0],&gps_c_struct.c_buff[gps_c_struct.comma_location[4]+1],gps_c_struct.comma_location[5]-gps_c_struct.comma_location[4]-1);//speed
							gpsflg = gpsflg & GPSFLG_SPD;
						}
					}							
				}
			}								
		}
		
		if(0x0F == gpsflg)
		{
			gpsflg = 0;	
			return TRUE;
		}
	}
	
	return FALSE;
}



void parse_gps_signal(void)	
{
#if DEBUG_GPS >0
	printf_va_args("s_latitude  = %s\n",gps_tmp_struct.tmp_latitude);
	printf_va_args("s_longitude = %s\n",gps_tmp_struct.tmp_longitude);
	printf_va_args("s_speed	  = %s\n",gps_tmp_struct.tmp_speed);
	printf_va_args("s_height	  = %s\n",gps_tmp_struct.tmp_height);
#endif
	memcpy(gps_tmp_struct.tmp_latitude_deg,gps_tmp_struct.tmp_latitude,2);
	memcpy(gps_tmp_struct.tmp_latitude_min,&gps_tmp_struct.tmp_latitude[2],7);

	memcpy(gps_tmp_struct.tmp_longitude_deg,gps_tmp_struct.tmp_longitude,3);
	memcpy(gps_tmp_struct.tmp_longitude_min,&gps_tmp_struct.tmp_longitude[3],7);

	gps_phy_struct.latitude  = (atof(gps_tmp_struct.tmp_latitude_min)/60)+atoi(gps_tmp_struct.tmp_latitude_deg);
	gps_phy_struct.longitude = (atof(gps_tmp_struct.tmp_longitude_min)/60)+atoi(gps_tmp_struct.tmp_longitude_deg);		
	gps_phy_struct.speed     = 1.852 *atof(gps_tmp_struct.tmp_speed);
	gps_phy_struct.height    = atof(gps_tmp_struct.tmp_height);
	
	if(g_sys_info.state_gps != 2)
		g_sys_info.state_gps = 1;		
}


tGPS_RCV_QUEUE g_gps_queue;

void fill_gps_queue(void)
{		
	unsigned short gpsDataLen;

	gpsDataLen = 32+8;
	g_gps_queue.qdata[g_gps_queue.wp].data[0] = UP_DAQ_SIGNAL;
	g_gps_queue.qdata[g_gps_queue.wp].data[1] = SIGNAL_GPS;
	g_gps_queue.qdata[g_gps_queue.wp].data[2] = 0xFF & (gpsDataLen+12);
	g_gps_queue.qdata[g_gps_queue.wp].data[3] = (gpsDataLen+12)>>8;

	g_gps_queue.qdata[g_gps_queue.wp].data[4] = 0x01;
	g_gps_queue.qdata[g_gps_queue.wp].data[5] = 0x00;
	g_gps_queue.qdata[g_gps_queue.wp].data[6] = 0x00;
	g_gps_queue.qdata[g_gps_queue.wp].data[7] = 0x00;

	memcpy(&g_gps_queue.qdata[g_gps_queue.wp].data[8],(void*)&US_SECOND,4);
	memcpy(&g_gps_queue.qdata[g_gps_queue.wp].data[12],(void*)&US_MILISECOND,2);
	
	memcpy(&g_gps_queue.qdata[g_gps_queue.wp].data[14],&gpsDataLen,2);
	
	memcpy(&g_gps_queue.qdata[g_gps_queue.wp].data[16],(void*)&gps_phy_struct.longitude,8);
	memcpy(&g_gps_queue.qdata[g_gps_queue.wp].data[24],(void*)&gps_phy_struct.latitude,8);
	memcpy(&g_gps_queue.qdata[g_gps_queue.wp].data[32],(void*)&gps_phy_struct.height,8);
	memcpy(&g_gps_queue.qdata[g_gps_queue.wp].data[40],(void*)&gps_phy_struct.speed,8);			

	g_gps_queue.qdata[g_gps_queue.wp].len = (gpsDataLen+12)+4;
	g_gps_queue.wp++;
	if(g_gps_queue.wp == GPS_RCV_QUEUE_SIZE)  
		g_gps_queue.wp = 0;	 
	g_gps_queue.cnt++;
}

/*********************************************************************************************************
    end file
*********************************************************************************************************/
