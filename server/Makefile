CC ?= gcc
CFLAGS ?= -Wall

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f aesdsocket *.o *.elf *.map

.PHONY: all clean
