#include "led.h"
#include "gpio.h"
#include "common.h"

void led_func(void)
{			
	if(g_edas_state.net_stat==1)
	{
		led_usb_blink();
	}
	else
	{
		edas_led_usbon();
	}
	edas_led_rxoff();
	edas_led_txoff();
	edas_led_koff();
	edas_led_erroff();
}

