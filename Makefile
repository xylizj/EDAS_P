EXEC = edas_p

OBJS = sd.o led.o can.o readcfg.o kline.o gpio.o gps.o net3g.o \
rtc.o upload_file.o common.o handle_file.o queue.o boot.o task.o power_monitor.o main.o

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
	-rm -f $(EXEC) *.o *~
