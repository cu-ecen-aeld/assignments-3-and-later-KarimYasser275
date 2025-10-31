CROSS_COMPILE?=
cc=$(CROSS_COMPILE)gcc

default : writer.c
	${cc} writer.c -o writer

clean:
	rm -f writer *.o *.elf *.map

.PHONY: clean