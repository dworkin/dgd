#
# Makefile for editor, part of DGD.
#
HOST=	SUNOS4
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	gcc
LD=	$(CC)
DMAKE=	make

OBJ=	line.o buffer.o file.o regexp.o vars.o edcmd.o cmdsub.o

a.out:	$(OBJ) ed.o
	cd ..; $(DMAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' ed.sub
	cd ../host; $(DMAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' \
			     sub
	$(LD) $(LDFLAGS) $(OBJ) ed.o `cat ../ed.sub` `cat ../host/sub` $(LIBS)

dgd:	$(OBJ)
	@for i in $(OBJ); do echo ed/$$i; done > dgd

clean:
	rm -f dgd a.out $(OBJ) ed.o


$(OBJ) ed.o: ../config.h ../host.h ../alloc.h ../error.h
file.o: ../path.h

$(OBJ) ed.o: ed.h
buffer.o file.o edcmd.o cmdsub.o ed.o: buffer.h
edcmd.o cmdsub.o ed.o: edcmd.h
cmdsub.o file.o: file.h
line.o buffer.o file.o edcmd.o cmdsub.o ed.o: line.h
regexp.o edcmd.o cmdsub.o ed.o: regexp.h
vars.o edcmd.o cmdsub.o ed.o: vars.h
