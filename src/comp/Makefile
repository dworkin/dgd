#
# Makefile for LPC precompiler, part of DGD.
#
HOST=	NETBSD
DEFINES=-D$(HOST)
DEBUG=
CCFLAGS=-O2 $(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../lex -I../kfun $(CCFLAGS)
LDFLAGS=
LIBS=
CC=	gcc
LD=	$(CC)
DMAKE=	make
YACC=	yacc

OBJ=	node.o parser.o control.o optimize.o codegeni.o compile.o csupport.o \
	codegenc.o comp.o
DGDOBJ=	node.o parser.o control.o optimize.o codegeni.o compile.o csupport.o
COMPOBJ=node.o parser.o control.o optimize.o codegenc.o compile.o comp.o

a.out:	$(COMPOBJ) always
	cd ..; $(DMAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp.sub
	cd ../lex; $(DMAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp
	cd ../kfun; $(DMAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp
	cd ../host; $(DMAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' \
			     sub
	$(LD) $(DEBUG) $(LDFLAGS) $(COMPOBJ) `cat ../comp.sub` \
	      `cat ../lex/comp` `cat ../kfun/comp` `cat ../host/sub` $(LIBS)

always:
	@rm -f a.out

dgd:	$(DGDOBJ)
	@for i in $(DGDOBJ); do echo comp/$$i; done > dgd

parser.c parser.h: parser.y
	$(YACC) -d parser.y
	mv y.tab.c parser.c
	mv y.tab.h parser.h

clean:
	rm -f dgd a.out $(OBJ)


$(OBJ) comp.o: ../dgd.h ../config.h ../host.h ../error.h ../alloc.h
$(OBJ) comp.o: ../str.h ../array.h ../xfloat.h ../interpret.h
control.o codegeni.o codegenc.o compile.o csupport.o comp.o: ../data.h
control.o codegeni.o codegenc.o compile.o csupport.o comp.o: ../fcontrol.h
control.o comp.o: ../hash.h ../path.h
comp.o: ../swap.h ../comm.h ../ed.h ../call_out.h

node.o parser.o compile.o: ../lex/macro.h ../lex/token.h
parser.o compile.o: ../lex/ppcontrol.h

control.o codegeni.o codegenc.o csupport.o: ../kfun/table.h

$(OBJ): comp.h
node.o parser.o optimize.o control.o codegeni.o codegenc.o: node.h
compile.o comp.o: node.h
control.o codegeni.o codegenc.o compile.o csupport.o: control.h
codegeni.o codegenc.o compile.o comp.o: codegen.h
parser.o control.o optimize.o codegenc.o compile.o comp.o: compile.h
optimize.o compile.o: optimize.h
csupport.o comp.o: csupport.h
