#
# Makefile for lexical scanner, part of DGD.
#
HOST=	DARWIN
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)
LDFLAGS=
LIBS=
LINTFLAGS=-abcehpruz
CC=	gcc
LD=	$(CC)

SRC=	macro.c ppstr.c token.c special.c ppcontrol.c
OBJ=	macro.o ppstr.o token.o special.o ppcontrol.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo lex/$$i; done > dgd

comp:	$(OBJ)
	@for i in $(OBJ); do echo ../lex/$$i; done > comp

lint:
	lint $(LINTFLAGS) $(CFLAGS) $(SRC)

clean:
	rm -f dgd comp $(OBJ)


$(OBJ): lex.h ../config.h ../host.h ../alloc.h ../str.h ../xfloat.h
macro.o special.o token.o ppcontrol.o: ../hash.h
token.o ppcontrol.o: ../path.h

$(OBJ): ../comp/node.h ../comp/compile.h ../comp/parser.h

$(OBJ): lex.h
macro.o special.o token.o ppcontrol.o: macro.h
ppstr.o token.o ppcontrol.o: ppstr.h
special.o token.o ppcontrol.o: special.h
token.o ppcontrol.o: token.h
ppcontrol.o: ppcontrol.h
