TARGET = aesdsocket

CFLAGS += -Wall
LDFLAGS += -lpthread -lrt

all: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o *.elf *.map

.PHONY: all clean
