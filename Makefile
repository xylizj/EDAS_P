EXEC = edas_p
OBJS = edas_p.o can.o readcfg.o kline.o edas_gpio.c gps.o net3g.o edas_rtc.o uploadingfile.o common.o handlefile.o LinkQueue.o boot.o

CROSS = arm-fsl-linux-gnueabi-
CC = $(CROSS)gcc
STRIP = $(CROSS)strip
#CFLAGS = -Wall -g 
#CFLAGS = -g 
all: clean $(EXEC)
$(EXEC):$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lpthread -lrt
	$(STRIP) $@

clean:
	-rm -f $(EXEC) *.o
