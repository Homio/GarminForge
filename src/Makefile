CC       = gcc
CFLAGS   = -Wall -O2 -D_FILE_OFFSET_BITS=64 -DGT_POSIX
LDFLAGS  = -lm
OUTDIR   = ../utils

OBJS = main.o garmin_transform.o gimglib.o util.o

.PHONY: all clean install

all: $(OUTDIR)/garmin_forge

$(OUTDIR)/garmin_forge: garmin_forge
	cp garmin_forge $(OUTDIR)/
	rm -f garmin_forge
	@echo "=== Installed: $(OUTDIR)/garmin_forge ==="

garmin_forge: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

main.o: main.c garmin_transform.h gimglib.h garmin_struct.h
garmin_transform.o: garmin_transform.c garmin_transform.h
gimglib.o: gimglib.c gimglib.h garmin_struct.h
util.o: util.c gimglib.h garmin_struct.h

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o garmin_forge

install: all
