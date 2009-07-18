#
# Makefile for kernel functions, part of DGD.
#
HOST=	DARWIN
DEFINES=-D$(HOST)
DEBUG=	-O -g
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../comp -I../parser $(CCFLAGS)
LINTFLAGS=-abcehpruz
CC=	gcc

SRC=	builtin.c std.c file.c math.c extra.c debug.c
OBJ=	builtin.o std.o file.o math.o extra.o debug.o

dgd:	table.o $(OBJ)
	@for i in table.o $(OBJ); do echo kfun/$$i; done > dgd

comp:	table.o $(OBJ)
	@for i in table.o $(OBJ); do echo ../kfun/$$i; done > comp

lint:
	lint $(LINTFLAGS) $(CFLAGS) $(SRC)

clean:
	rm -f dgd comp table.o $(OBJ)


$(OBJ): ../dgd.h ../config.h ../host.h ../alloc.h ../error.h ../str.h ../array.h
$(OBJ): ../object.h ../xfloat.h ../interpret.h ../data.h
std.o file.o: ../path.h ../editor.h
std.o: ../comm.h ../call_out.h
file.o: ../editor.h
extra.o: ../asn.h

std.o: ../comp/node.h ../comp/control.h ../comp/compile.h
debug.o: ../comp/control.h

extra.o: ../parser/parse.h

$(OBJ): kfun.h
builtin.o debug.o: table.h
table.o: table.h $(OBJ)
