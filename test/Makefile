# Network UPS Tools: common

CC	= gcc
CFLAGS	= -I../include -O -Wall -Wsign-compare -g
OBJ = ../common/tree.o ../common/nutparser.o ../common/stack.o ../common/data_types.o ../common/common.o

all: 
	$(CC) test_parser.c -o test_parser $(OBJ) $(CFLAGS)

clean:
	rm -f *~ *.o

distclean: clean

install: all