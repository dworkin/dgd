#
# This file is part of DGD, http://dgd-osr.sourceforge.net/
# Copyright (C) 1993-2010 Dworkin B.V.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
HOST=	DARWIN
DEFINES=-D$(HOST)
DEBUG=	-O -g
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../lex -I../parser -I../kfun $(CCFLAGS)
LDFLAGS=
LIBS=
LINTFLAGS=-abcehpruz
CC=	gcc
LD=	$(CC)
DMAKE=	make
YACC=	yacc

SRC=	node.c parser.c control.c optimize.c codegeni.c compile.c csupport.c \
	codegenc.c comp.c
OBJ=	node.o parser.o control.o optimize.o codegeni.o compile.o csupport.o \
	codegenc.o comp.o
DGDOBJ=	node.o parser.o control.o optimize.o codegeni.o compile.o csupport.o
COMPOBJ=node.o parser.o control.o optimize.o codegenc.o compile.o comp.o

a.out:	$(COMPOBJ) always
	cd ..; $(DMAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp.sub
	cd ../lex; $(DMAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp
	cd ../parser; $(DMAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp
	cd ../kfun; $(DMAKE) 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' comp
	cd ../host; $(DMAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' \
			     sub
	$(LD) $(DEBUG) $(LDFLAGS) $(COMPOBJ) `cat ../comp.sub` \
	      `cat ../lex/comp` `cat ../parser/comp` `cat ../kfun/comp` \
	      `cat ../host/sub` $(LIBS)

always:
	@rm -f a.out

lint:
	lint $(LINTFLAGS) $(CFLAGS) $(SRC)

dgd:	$(DGDOBJ)
	@for i in $(DGDOBJ); do echo comp/$$i; done > dgd

parser.c parser.h: parser.y
	$(YACC) -d parser.y
	mv y.tab.c parser.c
	mv y.tab.h parser.h

clean:
	rm -f dgd a.out $(OBJ)


$(OBJ) comp.o: ../dgd.h ../config.h ../host.h ../error.h ../alloc.h
$(OBJ) comp.o: ../str.h ../array.h ../object.h ../xfloat.h ../interpret.h
control.o optimize.o codegeni.o codegenc.o compile.o csupport.o: ../data.h
control.o comp.o: ../hash.h ../path.h
comp.o: ../data.h ../swap.h ../comm.h ../editor.h ../call_out.h

node.o parser.o compile.o: ../lex/macro.h ../lex/token.h
parser.o compile.o: ../lex/ppcontrol.h

control.o optimize.o codegeni.o codegenc.o csupport.o: ../kfun/table.h

$(OBJ): comp.h
node.o parser.o optimize.o control.o codegeni.o codegenc.o: node.h
compile.o comp.o csupport.o: node.h
control.o optimize.o codegeni.o codegenc.o compile.o csupport.o: control.h
codegeni.o codegenc.o compile.o comp.o: codegen.h
parser.o control.o optimize.o codegeni.o codegenc.o compile.o comp.o: compile.h
csupport.o: compile.h
optimize.o compile.o: optimize.h
csupport.o comp.o: csupport.h
