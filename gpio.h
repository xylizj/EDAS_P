#ifndef __GPIO_H_
#define __GPIO_H_


#define EDAS_POWERON 		11
#define EDAS_POWEROFF		10
#define EDAS_3G_WORK		20
#define EDAS_3G_OFF			21
#define EDAS_3G_START		31
#define EDAS_3G_STOP		30
#define EDAS_3G_RESTARTHI 	41
#define EDAS_3G_RESTARTLO	40

#define EDAS_LED_USBON		50
#define EDAS_LED_USBOFF		51

#define EDAS_LED_RXON		60
#define EDAS_LED_RXOFF		61

#define EDAS_LED_TXON		70
#define EDAS_LED_TXOFF		71

#define EDAS_LED_ERRON		80
#define EDAS_LED_ERROFF		81

#define EDAS_LED_KON		90
#define EDAS_LED_KOFF		91

#define EDAS_WD_ON			100
#define EDAS_WD_OFF			101


#define edas_power_on()		ioctl(fd_gpio, EDAS_POWERON)
#define edas_power_off()	ioctl(fd_gpio, EDAS_POWEROFF)

#define edas_3g_work()		ioctl(fd_gpio, EDAS_3G_WORK)
#define edas_3g_off()		ioctl(fd_gpio, EDAS_3G_OFF)

#define edas_3g_start()		ioctl(fd_gpio, EDAS_3G_START)
#define edas_3g_stop()		ioctl(fd_gpio, EDAS_3G_STOP)

#define edas_3g_restarthi()	ioctl(fd_gpio, EDAS_3G_RESTARTHI)
#define edas_3g_restartlo()	ioctl(fd_gpio, EDAS_3G_RESTARTLO)

#define edas_led_usbon()	ioctl(fd_gpio, EDAS_LED_USBON)
#define edas_led_usboff()	ioctl(fd_gpio, EDAS_LED_USBOFF)

#define edas_led_rxon()		ioctl(fd_gpio, EDAS_LED_RXON)
#define edas_led_rxoff()	ioctl(fd_gpio, EDAS_LED_RXOFF)

#define edas_led_txon()		ioctl(fd_gpio, EDAS_LED_TXON)
#define edas_led_txoff()	ioctl(fd_gpio, EDAS_LED_TXOFF)

#define edas_led_erron()	ioctl(fd_gpio, EDAS_LED_ERRON)
#define edas_led_erroff()	ioctl(fd_gpio, EDAS_LED_ERROFF)

#define edas_led_kon()		ioctl(fd_gpio, EDAS_LED_KON)
#define edas_led_koff()		ioctl(fd_gpio, EDAS_LED_KOFF)

#define edas_wd_on()		ioctl(fd_gpio, EDAS_WD_ON)
#define edas_wd_off()		ioctl(fd_gpio, EDAS_WD_OFF)


extern int fd_gpio;

int is_T15_on();
void init_gpio();
void init_adc();
void start3G();

void led_usb_blink();
void led_k_blink(int num);
void led_Tx_blink(int num);
void led_Rx_blink(int num);
void task_wd();


#endif/* __GPIO_H_*/
