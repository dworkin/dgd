#
# Makefile for host dependent object files for Unix
#
HOST=	SUN
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I../.. $(CCFLAGS)
CC=	gcc

OBJ=	local.o path.o dirent.o time.o alarm.o connect.o

host:	$(OBJ)

clean:
	rm -f $(OBJ)
