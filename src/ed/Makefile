#
# Makefile for editor, part of DGD.
#
HOST=	DARWIN
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
LDFLAGS=
LIBS=
LINTFLAGS=-abcehpruz
CC=	gcc
LD=	$(CC)

SRC=	line.c buffer.c fileio.c regexp.c vars.c edcmd.c cmdsub.c
OBJ=	line.o buffer.o fileio.o regexp.o vars.o edcmd.o cmdsub.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo ed/$$i; done > dgd

lint:
	lint $(LINTFLAGS) $(CFLAGS) $(SRC)

clean:
	rm -f dgd $(OBJ)


$(OBJ): ../config.h ../host.h ../alloc.h ../error.h
fileio.o: ../path.h

$(OBJ): ed.h
buffer.o fileio.o edcmd.o cmdsub.o: buffer.h
edcmd.o cmdsub.o: edcmd.h
cmdsub.o fileio.o: fileio.h
line.o buffer.o fileio.o edcmd.o cmdsub.o: line.h
regexp.o edcmd.o cmdsub.o: regexp.h
vars.o edcmd.o cmdsub.o: vars.h
