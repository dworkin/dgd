#
# Makefile for DGD, Dworkin's Game Driver
#
HOST=	SUN
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -Icomp -Ilex -Ied -Ikfun $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	gcc
LD=	$(CC)
MAKE=	make
BIN=	../bin

OBJ=	alloc.o error.o hash.o swap.o str.o array.o object.o data.o path.o \
	ed.o comm.o call_out.o interpret.o config.o dgd.o
EDOBJ=	alloc.o error.o
LEXOBJ=	alloc.o hash.o
COMPOBJ=alloc.o error.o hash.o path.o str.o array.o object.o data.o \
	interpret.o config.o

a.out:	$(OBJ)
	cd comp; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd lex; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd ed; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd kfun; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd lpc; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd host; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' host.a
	$(LD) $(LDFLAGS) $(OBJ) `cat comp/dgd` `cat lex/dgd` `cat ed/dgd` \
	      `cat kfun/dgd` `cat lpc/dgd` host/host.a $(LIBS)

$(BIN)/driver: a.out
	cp a.out $(BIN)/driver

$(BIN)/precomp: comp/a.out
	cp comp/a.out $(BIN)/precomp

install: $(BIN)/driver $(BIN)/precomp

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
	cd lpc; $(MAKE) clean
	cd host; $(MAKE) clean


config.o dgd.o: comp/node.h comp/compile.h
config.o interpret.o: comp/csupport.h
config.o: comp/parser.h

config.o: lex/macro.h lex/token.h lex/ppcontrol.h

ed.o:	ed/edcmd.h

interpret.o: kfun/table.h

$(OBJ):	dgd.h config.h host.h alloc.h error.h
error.o str.o array.o object.o data.o path.o comm.o: str.h array.h object.h
ed.o call_out.o interpret.o config.o dgd.o: str.h array.h object.h
array.o object.o data.o path.o ed.o comm.o call_out.o: interpret.h
interpret.o config.o dgd.o: interpret.h
str.o array.o object.o data.o call_out.o interpret.o config.o: data.h
interpret.o: fcontrol.h
path.o config.o: path.h
hash.o str.o: hash.h
swap.o data.o: swap.h
ed.o config.o: ed.h
call_out.o config.o dgd.o: call_out.h
error.o comm.o config.o dgd.o: comm.h
