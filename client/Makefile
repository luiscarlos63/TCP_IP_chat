
CC=arm-buildroot-linux-gnueabihf-gcc
CFLAGS= -g -pthread -lrt

all:EMB_tcpclient EMB_send_message

EMB_tcpclient:  
	$(CC) -o EMB_tcpclient.elf tcpclient.c $(CFLAGS)

EMB_send_message: 
	$(CC) -o EMB_send_message.elf send_message.c $(CFLAGS)

.PHONY: clean
clean:
	rm -rf *.elf *.o

