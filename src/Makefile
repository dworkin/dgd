#
# Makefile for DGD, Dworkin's Game Driver
#
DEFINES=-DSUN -DRUSAGE
DEBUG=	-g
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -Icomp -Ilex -Ied -Ikfun $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	gcc
LD=	$(CC)
MAKE=	make
BIN=	../bin

OBJ=	host.o alloc.o error.o hash.o path.o swap.o str.o array.o object.o \
	data.o comm.o interpret.o dgd.o
EDOBJ=	host.o alloc.o error.o
LEXOBJ=	host.o alloc.o hash.o
COMPOBJ=host.o alloc.o error.o hash.o path.o swap.o str.o array.o object.o \
	data.o comm.o

a.out:	$(OBJ)
	cd comp; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd lex; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd ed; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd kfun; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	$(LD) $(LDFLAGS) $(OBJ) `cat comp/dgd` `cat lex/dgd` `cat ed/dgd` \
	`cat kfun/dgd` $(LIBS)

$(BIN)/driver: a.out
	@mv -f $(BIN)/driver $(BIN)/driver.old
	cp a.out $(BIN)/driver

$(BIN)/precomp: comp/a.out
	cp comp/a.out $(BIN)/precomp

all:	$(BIN)/driver $(BIN)/precomp

comp.sub: $(COMPOBJ)
	@for i in $(COMPOBJ); do echo ../$$i; done > comp.sub

lex.sub: $(LEXOBJ)
	@for i in $(LEXOBJ); do echo ../$$i; done > lex.sub

ed.sub:	$(EDOBJ)
	@for i in $(EDOBJ); do echo ../$$i; done > ed.sub

comp/a.out:
	cd comp; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' a.out

clean:
	rm -f a.out $(OBJ) comp.sub lex.sub ed.sub
	cd comp; $(MAKE) clean
	cd lex; $(MAKE) clean
	cd ed; $(MAKE) clean
	cd kfun; $(MAKE) clean
