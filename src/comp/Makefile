#
# Makefile for LPC precompiler, part of DGD.
#
HOST=	SUN
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../lex -I../kfun $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	gcc
LD=	$(CC)
MAKE=	make
YACC=	yacc

OBJ=	node.o parser.o control.o compile.o codegeni.o csupport.o codegenc.o \
	comp.o
DGDOBJ=	node.o parser.o control.o compile.o codegeni.o csupport.o
COMPOBJ=node.o parser.o control.o compile.o codegenc.o comp.o

a.out:	$(COMPOBJ)
	cd ..; $(MAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp.sub
	cd ../lex; $(MAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp
	cd ../kfun; $(MAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp
	cd ../host; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' \
			    host.a
	$(LD) $(LDFLAGS) $(COMPOBJ) `cat ../comp.sub` `cat ../lex/comp` \
	      `cat ../kfun/comp` ../host/host.a $(LIBS)

dgd:	$(DGDOBJ)
	@for i in $(DGDOBJ); do echo comp/$$i; done > dgd

parser.c parser.h: parser.y parser.proto
	$(YACC) -d parser.y
	mv y.tab.c parser.c
	mv y.tab.h parser.h
	cat parser.proto >> parser.h

clean:
	rm -f dgd a.out $(OBJ)


$(OBJ): ../dgd.h ../config.h ../host.h ../error.h ../alloc.h ../str.h
$(OBJ): ../array.h ../interpret.h
control.o compile.o codegeni.o codegenc.o csupport.o: ../data.h ../fcontrol.h
control.o comp.o: ../hash.h
compile.o comp.o: ../path.h
comp.o: ../swap.h ../comm.h ../ed.h ../call_out.h

node.o parser.o compile.o: ../lex/macro.h ../lex/token.h
parser.o compile.o: ../lex/ppcontrol.h

control.o codegeni.o codegenc.o csupport.o: ../kfun/table.h

$(OBJ): comp.h
node.o parser.o compile.o codegeni.o codegenc.o comp.o: node.h
control.o compile.o codegeni.o codegenc.o csupport.o: control.h
compile.o codegeni.o codegenc.o: codegen.h
parser.o compile.o comp.o: compile.h
csupport.o comp.o: csupport.h
