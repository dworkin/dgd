#
# Makefile for editor, part of DGD.
#
HOST=	ATARI_ST
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)
LDFLAGS=$(CCFLAGS)
LIBS=
CC=	gcc
LD=	$(CC)
SYMLD=	$(CC) -B/usr/bin/sym-
SHELL=	/bin/sh.ttp
DMAKE=	make

OBJ=	macro.o ppstr.o token.o special.o ppcontrol.o

a.out:	$(OBJ) lex.o
	cd ..; $(DMAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' lex.sub
	cd ../host; $(DMAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' \
			     sub
	$(LD) $(LDFLAGS) $(OBJ) lex.o `cat ../lex.sub` `cat ../host/sub` $(LIBS)
	fixstk 64K $@

debug:	a.out
	$(SYMLD) $(LDFLAGS) $(OBJ) lex.o `cat ../lex.sub` `cat ../host/sub` \
	$(LIBS) -o $@

dgd:	$(OBJ)
	@for i in $(OBJ); do echo lex/$$i; done > dgd

comp:	$(OBJ)
	@for i in $(OBJ); do echo ../lex/$$i; done > comp

clean:
	rm -f dgd comp a.out $(OBJ) lex.o


$(OBJ) lex.o: lex.h ../config.h ../host.h ../alloc.h ../str.h ../xfloat.h
macro.o special.o token.o ppcontrol.o lex.o: ../hash.h
ppcontrol.o: ../path.h

$(OBJ) lex.o: ../comp/node.h ../comp/compile.h ../comp/parser.h

$(OBJ) lex.o: lex.h
macro.o special.o token.o ppcontrol.o lex.o: macro.h
ppstr.o token.o ppcontrol.o: ppstr.h
special.o token.o ppcontrol.o: special.h
token.o ppcontrol.o lex.o: token.h
ppcontrol.o lex.o: ppcontrol.h
