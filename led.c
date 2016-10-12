#include "led.h"
#include "gpio.h"


void led_off(void)
{		
	edas_led_rxoff();
	edas_led_txoff();
	edas_led_koff();
	edas_led_erroff();
}


void led_usb_blink(void)
{
	static char led_usb_state=0;
	if(led_usb_state)
	{
		edas_led_usbon();
		led_usb_state=0;
	}
	else
	{
		edas_led_usboff();
		led_usb_state=1;
	}
}

void led_k_blink(int num)
{
	static int led_k_state=0;
	static int cnt = 0;
	if(cnt < num) 
	{
		cnt++;
	}
	else
	{
		cnt  = 0;
		if(led_k_state)
		{
			edas_led_kon();
			led_k_state=0;
		}
		else
		{
			edas_led_koff();
			led_k_state=1;
		}
	}
	
}

void led_Rx_blink(int num)
{
	static int led_rx_state=0;
	static int cnt = 0;
	if(cnt < num) 
	{
		cnt++;
	}
	else
	{
		cnt  = 0;
		if(led_rx_state)
		{
			edas_led_rxon();
			led_rx_state=0;
		}
		else
		{
			edas_led_rxoff();
			led_rx_state=1;
		}
	}
	
}
void led_Tx_blink(int num)
{
	static int led_tx_state=0;
	static int cnt = 0;

	if(cnt < num) 
	{
		cnt++;
	}
	else
	{
		cnt=0;
		if(led_tx_state)
		{
			edas_led_txon();
			led_tx_state=0;
		}
		else
		{
			edas_led_txoff();
			led_tx_state=1;
		}
	}	
}
