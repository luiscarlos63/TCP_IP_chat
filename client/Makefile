CC=arm-buildroot-linux-gnueabihf-gcc
CFLAGS= -g -pthread

all:EMB_tcpclient.o EMB_tcpclient


EMB_tcpclient.o: tcpclient.c 
	$(CC) -c -o EMB_tcpclient.o tcpclient.c $(CFLAGS)

EMB_tcpclient: EMB_tcpclient.o
	$(CC) -o EMB_tcpclient.elf EMB_tcpclient.o $(CFLAGS)



.PHONY: clean
clean:
	rm -rf EMB_tcpclient.elf *.o