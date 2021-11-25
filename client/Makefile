CC=gcc#arm-buildroot-linux-gnueabihf-gcc
CFLAGS= -g -pthread

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

tcpclient: tcpclient.o
	$(CC) -o tcpclient tcpclient.o $(CFLAGS)



.PHONY: clean
clean:
	rm -rf tcpclient *.o