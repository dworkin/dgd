#
# Makefile for DGD, Dworkin's Game Driver
#
DEFINES=-DMINIX_68K
DEBUG=
CCFLAGS=-O $(DEFINES) $(DEBUG)
CFLAGS=	-I. -Ikfun -Ilex -Icomp -Ied $(CCFLAGS)
LDFLAGS=-m200000
LIBS=
CC=	cc
LD=	cc
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
	cd kfun; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	$(LD) $(LDFLAGS) $(OBJ) `cat comp/dgd` `cat lex/dgd` `cat kfun/dgd` \
	$(LIBS)

$(BIN)/driver: a.out
	@mv -f $(BIN)/driver $(BIN)/driver.old
	cp a.out $(BIN)/driver

$(BIN)/precomp: comp/a.out
	cp comp/a.out $(BIN)/precomp

all:	$(BIN)/driver $(BIN)/precomp

ed.sub:	$(EDOBJ)
	@for i in $(EDOBJ); do echo ../$$i; done > ed.sub

lex.sub: $(LEXOBJ)
	@for i in $(LEXOBJ); do echo ../$$i; done > lex.sub

comp.sub: $(COMPOBJ)
	@for i in $(COMPOBJ); do echo ../$$i; done > comp.sub

comp/a.out:
	cd comp; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' a.out

clean:
	rm -f a.out $(OBJ) ed.sub lex.sub comp.sub
	cd ed; $(MAKE) clean
	cd comp; $(MAKE) clean
	cd lex; $(MAKE) clean
	cd kfun; $(MAKE) clean
