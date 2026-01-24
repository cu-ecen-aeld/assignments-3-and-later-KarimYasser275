CC ?= gcc
CFLAGS ?= -g -Wall
LDFLAGS ?= -lpthread -lrt
TARGET ?= aesdsocket

all: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o *.elf *.map

.PHONY: all clean
