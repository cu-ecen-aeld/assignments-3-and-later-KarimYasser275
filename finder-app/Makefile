CC ?= gcc
CFLAGS ?= -Wall -Werror

all: writer

writer: writer.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f writer *.o *.elf *.map

.PHONY: all clean
