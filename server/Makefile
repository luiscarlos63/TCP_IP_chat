CC=gcc#arm-buildroot-linux-gnueabihf-gcc
CFLAGS= -g -pthread

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

tcpserver: tcpserver.o
	$(CC) -o tcpserver tcpserver.o $(CFLAGS)



.PHONY: clean
clean:
	rm -rf tcpserver *.o