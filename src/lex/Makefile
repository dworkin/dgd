#
# Makefile for lexical scanner, part of DGD.
#
HOST=	SUN
DEFINES=-D$(HOST)
DEBUG=	-g
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	gcc
LD=	$(CC)
MAKE=	make

OBJ=	macro.o ppstr.o token.o special.o ppcontrol.o

a.out:	$(OBJ) lex.o
	cd ../host; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' \
			    host.a
	cd ..; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' lex.sub
	$(LD) $(LDFLAGS) $(OBJ) lex.o `cat ../lex.sub` ../host/host.a $(LIBS)

dgd:	$(OBJ)
	@for i in $(OBJ); do echo lex/$$i; done > dgd

comp:	$(OBJ)
	@for i in $(OBJ); do echo ../lex/$$i; done > comp

clean:
	rm -f dgd comp a.out $(OBJ) lex.o


$(OBJ) lex.o: lex.h ../config.h ../alloc.h
ppcontrol.o: ../version.h
macro.o special.o token.o ppcontrol.o: ../hash.h
token.o: ../path.h

macro.o special.o token.o ppcontrol.o: macro.h
ppstr.o token.o ppcontrol.o: ppstr.h
special.o token.o ppcontrol.o: special.h
token.o ppcontrol.o lex.o: token.h
ppcontrol.o lex.o: ppcontrol.h
