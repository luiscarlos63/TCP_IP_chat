CC=gcc#arm-buildroot-linux-gnueabihf-gcc
CFLAGS= -g -pthread -lrt


%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

all: tcpclient send_out


tcpclient: tcpclient.o
	$(CC) -o tcpclient.elf tcpclient.o $(CFLAGS)

send_out: send_out.o
	$(CC) -o send_out.elf send_out.o $(CFLAGS)

.PHONY: clean
clean:
	rm -rf *.elf *.o