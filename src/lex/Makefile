#
# Makefile for lexical scanner, part of DGD.
#
DEFINES=-DMINIX_68K
DEBUG=
CCFLAGS=-O $(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	cc
LD=	cc
MAKE=	make

OBJ=	macro.o ppstr.o token.o special.o ppcontrol.o

a.out:	$(OBJ) lex.o
	cd ..; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' lex.sub
	$(LD) $(LDFLAGS) $(OBJ) `cat ../lex.sub` lex.o $(LIBS)

dgd:	$(OBJ)
	@for i in $(OBJ); do echo lex/$$i; done > dgd

comp:	$(OBJ)
	@for i in $(OBJ); do echo ../lex/$$i; done > comp

clean:
	rm -f dgd comp a.out $(OBJ) lex.o


../comp/parser.h:
	cd ../comp; $(MAKE) parser.h

$(OBJ) lex.o: lex.h ../config.h ../host.h ../alloc.h ../comp/parser.h
macro.o special.o token.o ppcontrol.o: ../hash.h
token.o: ../path.h

macro.o special.o token.o ppcontrol.o: macro.h
ppstr.o token.o ppcontrol.o: ppstr.h
special.o token.o ppcontrol.o: special.h
token.o ppcontrol.o lex.o: token.h
ppcontrol.o lex.o: ppcontrol.h
