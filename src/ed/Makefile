#
# Makefile for editor, part of DGD.
#
HOST=	NETBSD
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	gcc
LD=	$(CC)
DMAKE=	make

OBJ=	line.o buffer.o fileio.o regexp.o vars.o edcmd.o cmdsub.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo ed/$$i; done > dgd

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
