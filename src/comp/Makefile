#
# Makefile for LPC compiler, part of DGD.
#
HOST=	SUN
DEFINES=-D$(HOST)
DEBUG=	-g
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../lex -I../kfun $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	gcc
LD=	$(CC)
MAKE=	make
YACC=	yacc

OBJ=	node.o parser.o control.o compile.o

a.out:	$(OBJ) codegenc.o comp.o
	cd ../host; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' \
			    host.a
	cd ..; $(MAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp.sub
	cd ../lex; $(MAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp
	cd ../kfun; $(MAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp
	$(LD) $(LDFLAGS) $(OBJ) codegenc.o comp.o `cat ../comp.sub` \
	      `cat ../lex/comp` `cat ../kfun/comp` ../host/host.a $(LIBS)

dgd:	$(OBJ) codegeni.o
	@for i in $(OBJ) codegeni.o; do echo comp/$$i; done > dgd

clean:
	rm -f dgd a.out $(OBJ) codegeni.o codegenc.o comp.o


parser.c parser.h: parser.y parser.proto
	$(YACC) -d parser.y
	mv y.tab.c parser.c
	mv y.tab.h parser.h
	cat parser.proto >> parser.h

$(OBJ) codegeni.o codegenc.o comp.o: comp.h ../config.h ../alloc.h ../error.h
$(OBJ) codegeni.o codegenc.o comp.o: ../str.h ../interpret.h
parser.o control.o compile.o codegeni.o codegenc.o comp.o: ../object.h ../data.h
node.o parser.o compile.o: ../lex.h ../lex/token.h ../lex/ppcontrol.h
control.o codegeni.o codegenc.o comp.o: ../kfun.h ../kfun/table.h
control.o compile.o fcontrol.o: ../fcontrol.h
control.o: ../hash.h
compile.o: ../path.h
node.o parser.o compile.o codegeni.o codegenc.o comp.o: node.h
control.o codegeni.o codegenc.o compile.o: control.h
codegeni.o codegenc.o compile.o: codegen.h
parser.o compile.o comp.o: compile.h
