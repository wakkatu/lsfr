.PHONY: clean check

CFLAGS += -g -Wall

lsfr: lsfr.c

clean:
	rm -f lsfr

check:
	./verify
