#
# Makefile for string parser, part of DGD.
#
HOST=	DARWIN
DEFINES=-D$(HOST)
DEBUG=	-O -g
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
LINTFLAGS=-abcehpruz
CC=	gcc

SRC=	grammar.c dfa.c srp.c parse.c
OBJ=	grammar.o dfa.o srp.o parse.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo parser/$$i; done > dgd

comp:	$(OBJ)
	@for i in $(OBJ); do echo ../parser/$$i; done > comp

lint:
	lint $(LINTFLAGS) $(CFLAGS) $(SRC)

clean:
	rm -f dgd comp $(OBJ)


$(OBJ): ../dgd.h ../config.h ../host.h ../alloc.h ../error.h
grammar.o dfa.o: ../hash.h ../str.h
parse.o: ../str.h ../array.h ../object.h ../interpret.h ../data.h

grammar.o parse.o: grammar.h
dfa.o parse.o: dfa.h
srp.o parse.o: srp.h
parse.o: parse.h
