#
# Makefile for editor, part of DGD.
#
DEFINES=-DSUN
DEBUG=	-g
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	gcc
LD=	$(CC)
MAKE=	make

OBJ=	line.o buffer.o file.o regexp.o vars.o edcmd.o cmdsub.o

a.out:	$(OBJ) ed.o
	cd ..; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' ed.sub
	$(LD) $(LDFLAGS) $(OBJ) `cat ../ed.sub` ed.o $(LIBS)

dgd:	$(OBJ)
	@for i in $(OBJ); do echo ed/$$i; done > dgd

clean:
	rm -f dgd a.out $(OBJ) ed.o


$(OBJ) ed.o: ed.h ../config.h ../host.h ../alloc.h ../error.h
file.o: ../path.h

edcmd.o cmdsub.o file.o buffer.o: buffer.h
ed.o edcmd.o cmdsub.o: edcmd.h
cmdsub.o file.o: file.h
edcmd.o cmdsub.o file.o buffer.o line.o: line.h
ed.o edcmd.o cmdsub.o regexp.o: regexp.h
ed.o edcmd.o cmdsub.o vars.o: vars.h
