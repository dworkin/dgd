#
# Makefile for string parser, part of DGD.
#
HOST=	NETBSD
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)
CC=	gcc

OBJ=	grammar.o dfa.o srp.o parse.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo parser/$$i; done > dgd

comp:	$(OBJ)
	@for i in $(OBJ); do echo ../parser/$$i; done > comp

clean:
	rm -f dgd comp $(OBJ)


$(OBJ): ../dgd.h ../config.h ../host.h ../alloc.h ../error.h
grammar.o dfa.o: ../hash.h ../str.h
srp.o: ../str.h
parse.o: ../str.h ../array.h ../object.h ../interpret.h ../data.h

grammar.o parse.o: grammar.h
dfa.o parse.o: dfa.h
srp.o parse.o: srp.h
parse.o: parse.h
