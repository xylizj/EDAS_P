EXEC = edas_p

OBJS = common.o sd.o boot.o readcfg.o queue.o gpio.o led.o gps.o rtc.o monitor.o can.o kline.o  \
net3g.o recfile.o upload_file.o task.o  main.o

CROSS = arm-fsl-linux-gnueabi-
CC = $(CROSS)gcc
STRIP = $(CROSS)strip
CFLAGS = -Wall -g 
#CFLAGS = -g 
all: clean $(EXEC)
$(EXEC):$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lpthread -lrt
	$(STRIP) $@

clean:
	-rm -f $(EXEC) *.o *~
