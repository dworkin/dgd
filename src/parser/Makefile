#
# Makefile for string parser, part of DGD.
#
HOST=	NETBSD
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)
CC=	gcc

OBJ=	grammar.o dfa.o pda.o parse.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo parser/$$i; done > dgd

comp:	$(OBJ)
	@for i in $(OBJ); do echo ../parser/$$i; done > comp

clean:
	rm -f dgd comp $(OBJ)


$(OBJ): ../dgd.h ../config.h ../host.h ../alloc.h ../error.h ../str.h
grammar.o dfa.o: ../hash.h
parse.o: ../data.h

grammar.o parse.o: grammar.h
dfa.o parse.o: dfa.h
pda.o parse.o: pda.h
