#
# Makefile for host dependent object files for Unix
#
HOST=	SUN # override this with the proper host
DEFINES=-D$(HOST)
DEBUG=	-g
CCFLAGS=-O $(DEFINES) $(DEBUG)
CFLAGS=	-I. -I../.. $(CCFLAGS)
CC=	gcc
MAKE=	make

OBJ=	local.o path.o dirent.o connect.o

host:	$(OBJ)

clean:
	rm -f $(OBJ) connect.c
