#
# Makefile for kernel functions, part of DGD.
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
YACC=	yacc

OBJ=	builtin.o std.o file.o extra.o debug.o

dgd:	table.o $(OBJ)
	@for i in table.o $(OBJ); do echo kfun/$$i; done > dgd

comp:	table.o $(OBJ)
	@for i in table.o $(OBJ); do echo ../kfun/$$i; done > comp

clean:
	rm -f dgd comp table.o $(OBJ)


$(OBJ): ../config.h ../alloc.h ../error.h ../str.h ../array.h ../object.h
$(OBJ): ../data.h ../interpret.h
table.o: table.h $(OBJ)
