# set the path to pin if not set in .bachrc
PIN_DIR=$(PIN_ROOT)

# set the path to mcprof if not set in .bachrc
#MCPROF_DIR=/data/home/in4342-admin/mcprof
MCPROF_DIR=/home/ubuntu/mcprof

# set the path to the toolchain for cross-compilation for arm
CC =/data/usr/local/share/codesourcery/bin/arm-none-linux-gnueabi-gcc
#CC = gcc
INC = -I.
#CFLAGS =-O2 -Wall
#CFLAGS = -O3 -Wall
CFLAGS = -O3 -Wall -march=armv7-a -mtune=cortex-a8 -mfpu=vfp -ftree-vectorize -mfloat-abi=softfp
LFLAGS = -L.
LIBS = -lm
LDFLAGS=

CSRCS 	= canny_edge.c hysteresis.c pgm_io.c Timer.c
OBJS	=$(CSRCS:%.c=%.o)

# Use one of the following pictures
PIC	=./pics/klomp.pgm
#PIC	=./pics/square.pgm
#PIC	=./pics/tiger.pgm

EXEC    = canny
CMD	= ./$(EXEC) $(PIC)

all: clean $(EXEC) run

$(EXEC) : $(OBJS)
	$(CC) $(OBJS) -o $(EXEC) $(LFLAGS) $(LDFLAGS) $(LIBS)

%.o : %.c
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@ 

run: $(EXEC) $(PIC)
	$(CMD)

debug: CFLAGS= -g -Wall
debug: clean $(EXEC) $(PIC)
	gdb --args $(CMD)

#gprof: CFLAGS=-O2 -g -pg -Wall
gprof: CFLAGS=-O3 -g -pg -Wall -march=armv7-a -mtune=cortex-a8 -mfpu=vfp -ftree-vectorize -mfloat-abi=softfp
gprof: LDFLAGS=-pg
gprof: clean $(EXEC)
	$(CMD)
	gprof -b $(EXEC) > gprof.out

MCPROF_OPT:=-RecordStack 0  -TrackObjects 1 -Engine 2 -TrackStartStop 1 -StaticSymbols 0 -TrackZones 0 -Threshold 100
mcprof: CFLAGS= -O1 -g -fno-inline -fno-omit-frame-pointer -Wall
mcprof: clean $(EXEC)
	$(PIN_DIR)/pin -t $(MCPROF_DIR)/obj-intel64/mcprof.so $(MCPROF_OPT) -- $(CMD)
	$(MCPROF_DIR)/scripts/callgraph2pdf.sh
	$(MCPROF_DIR)/scripts/dot2pdf.sh communication.dot
	$(MCPROF_DIR)/scripts/plotScript.sh

clean:
	rm -f $(EXEC) $(OBJS) *~ gmon.out pin.log *.pdf *.out *.dot *.json *.eps

.PHONY: clean canny run all debug gprof mcprof
