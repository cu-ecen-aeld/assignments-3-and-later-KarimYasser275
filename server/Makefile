CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall
LDFLAGS ?= -lpthread -lrt
TARGET ?= aesdsocket

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f aesdsocket *.o *.elf *.map

.PHONY: all clean
