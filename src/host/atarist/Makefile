#
# Makefile for DGD, Dworkin's Game Driver
#
HOST=	ATARI_ST
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -Icomp -Ilex -Ied -Ikfun $(CCFLAGS)
LDFLAGS=$(CCFLAGS)
LIBS=
CC=	gcc
LD=	$(CC)
SYMLD=	$(CC) -B/usr/bin/sym-
SHELL=	/bin/sh.ttp
DMAKE=	make
BIN=	../bin

OBJ=	alloc.o error.o hash.o swap.o str.o array.o object.o data.o path.o \
	ed.o comm.o call_out.o interpret.o config.o dgd.o
EDOBJ=	alloc.o error.o
LEXOBJ=	alloc.o hash.o
COMPOBJ=alloc.o error.o hash.o path.o str.o array.o object.o data.o \
	interpret.o config.o

a.out:	$(OBJ)
	cd comp; $(MAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd lex; $(MAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd ed; $(MAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd kfun; $(MAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd lpc; $(MAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	cd host; $(MAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' dgd
	$(LD) $(LDFLAGS) $(OBJ) `cat comp/dgd` `cat lex/dgd` `cat ed/dgd` \
	      `cat kfun/dgd` `cat lpc/dgd` `cat host/dgd` $(LIBS)
	fixstk 64K $@

debug:	a.out
	$(SYMLD) $(LDFLAGS) $(OBJ) `cat comp/dgd` `cat lex/dgd` `cat ed/dgd` \
		 `cat kfun/dgd` `cat lpc/dgd` `cat host/dgd` $(LIBS) -o $@

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
	cd comp; $(DMAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' a.out

clean:
	rm -f a.out $(OBJ) comp.sub lex.sub ed.sub
	cd comp; $(DMAKE) clean
	cd lex; $(DMAKE) clean
	cd ed; $(DMAKE) clean
	cd kfun; $(DMAKE) clean
	cd lpc; $(DMAKE) clean
	cd host; $(DMAKE) clean


path.o config.o dgd.o: comp/node.h comp/compile.h
config.o data.o interpret.o: comp/csupport.h
config.o: comp/parser.h

config.o: lex/macro.h lex/token.h lex/ppcontrol.h

ed.o:	ed/edcmd.h

interpret.o: kfun/table.h

$(OBJ):	dgd.h config.h host.h alloc.h error.h
error.o str.o array.o object.o data.o path.o comm.o: str.h array.h object.h
ed.o call_out.o interpret.o config.o dgd.o: str.h array.h object.h
array.o interpret.o path.o config.o dgd.o: xfloat.h
array.o object.o data.o path.o ed.o comm.o call_out.o: interpret.h
interpret.o config.o dgd.o: interpret.h
str.o array.o object.o data.o path.o call_out.o interpret.o: data.h
config.o dgd.o: data.h
interpret.o: fcontrol.h
path.o config.o: path.h
hash.o str.o: hash.h
swap.o data.o: swap.h
ed.o config.o dgd.o: ed.h
call_out.o config.o dgd.o: call_out.h
error.o comm.o call_out.o config.o dgd.o: comm.h
config.o: version.h
