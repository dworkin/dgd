#
# This file is part of DGD, https://github.com/dworkin/dgd
# Copyright (C) 1993-2010 Dworkin B.V.
# Copyright (C) 2010-2023 DGD Authors (see the commit log for details)
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
CXXFLAGS=-I. -I.. -I../lex -I../parser -I../kfun $(CCFLAGS)

SRC=	node.cpp parser.cpp control.cpp optimize.cpp codegen.cpp compile.cpp
OBJ=	node.o parser.o control.o optimize.o codegen.o compile.o

all:
	@echo Please run make from the src directory.

dgd:	$(OBJ)
	@for i in $(OBJ); do echo comp/$$i; done > dgd

parser.cpp parser.h: parser.y
	$(YACC) -d parser.y
	mv y.tab.c parser.cpp
	mv y.tab.h parser.h

clean:
	rm -f dgd a.out $(OBJ)


$(OBJ): ../dgd.h ../config.h ../host.h ../error.h ../alloc.h ../str.h
$(OBJ): ../array.h ../object.h ../hash.h ../swap.h ../xfloat.h ../interpret.h
node.o parser.o control.o optimize.o codegen.o compile.o: ../data.h
compile.o: ../path.h
codegen.o: ../ext.h

node.o parser.o compile.o: ../lex/ppcontrol.h

parser.o control.o optimize.o codegen.o: ../kfun/table.h

$(OBJ): comp.h node.h
parser.o control.o optimize.o codegen.o compile.o: control.h
codegen.o compile.o: codegen.h
parser.o control.o optimize.o codegen.o compile.o: compile.h
optimize.o compile.o: optimize.h
