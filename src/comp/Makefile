#
# This file is part of DGD, https://github.com/dworkin/dgd
# Copyright (C) 1993-2010 Dworkin B.V.
# Copyright (C) 2010-2015 DGD Authors (see the commit log for details)
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
CFLAGS=	-I. -I.. -I../lex -I../parser -I../kfun $(CCFLAGS)

SRC=	node.c parser.c control.c optimize.c codegeni.c compile.c csupport.c
OBJ=	node.o parser.o control.o optimize.o codegeni.o compile.o csupport.o

all:
	@echo Please run make from the src directory.

dgd:	$(OBJ)
	@for i in $(OBJ); do echo comp/$$i; done > dgd

parser.c parser.h: parser.y
	$(YACC) -d parser.y
	mv y.tab.c parser.c
	mv y.tab.h parser.h

clean:
	rm -f dgd a.out $(OBJ)


$(OBJ): ../dgd.h ../config.h ../host.h ../error.h ../alloc.h
$(OBJ): ../str.h ../array.h ../object.h ../xfloat.h ../interpret.h
control.o optimize.o codegeni.o compile.o csupport.o: ../data.h
control.o: ../hash.h ../path.h

node.o parser.o compile.o: ../lex/macro.h ../lex/token.h
parser.o compile.o: ../lex/ppcontrol.h

control.o optimize.o codegeni.o csupport.o: ../kfun/table.h

$(OBJ): comp.h
node.o parser.o optimize.o control.o codegeni.o: node.h
compile.o csupport.o: node.h
control.o optimize.o codegeni.o compile.o csupport.o: control.h
codegeni.o compile.o: codegen.h
parser.o control.o optimize.o codegeni.o compile.o: compile.h
csupport.o: compile.h
optimize.o compile.o: optimize.h
csupport.o: csupport.h
