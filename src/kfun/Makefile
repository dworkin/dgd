#
# Makefile for kernel functions, part of DGD.
#
HOST=	SUN
DEFINES=-D$(HOST)
DEBUG=	-g
CCFLAGS=-O $(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	gcc
LD=	$(CC)
MAKE=	make
YACC=	yacc

OBJ=	builtin.o std.o file.o extra.o debug.o

dgd:	table.o $(OBJ)
	@for i in table.o $(OBJ); do echo kfun/$$i; done > dgd

comp:	table.o $(OBJ)
	@for i in table.o $(OBJ); do echo ../kfun/$$i; done > comp

clean:
	rm -f dgd comp table.o $(OBJ)


$(OBJ): ../config.h ../host.h ../alloc.h ../error.h ../interpret.h
$(OBJ): ../str.h ../array.h ../object.h ../data.h
table.o: $(OBJ)
