CC=gcc#arm-buildroot-linux-gnueabihf-gcc
CFLAGS= -g -pthread -lrt


%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

all: tcpclient send_message


tcpclient: tcpclient.o
	$(CC) -o tcpclient.elf tcpclient.o $(CFLAGS)

send_message: send_message.o
	$(CC) -o send_message.elf send_message.o $(CFLAGS)

.PHONY: clean
clean:
	rm -rf *.elf *.o