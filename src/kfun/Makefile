#
# Makefile for kernel functions, part of DGD.
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
YACC=	yacc

OBJ=	table.o builtin.o std.o file.o extra.o debug.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo kfun/$$i; done > dgd

comp:	$(OBJ)
	@for i in $(OBJ); do echo ../kfun/$$i; done > comp

clean:
	rm -f dgd comp $(OBJ)


$(OBJ): ../config.h ../host.h ../alloc.h ../error.h ../interpret.h ../str.h
$(OBJ): ../array.h ../object.h ../data.h
