#
# This file is part of DGD, https://github.com/dworkin/dgd
# Copyright (C) 1993-2010 Dworkin B.V.
# Copyright (C) 2010-2021 DGD Authors (see the commit log for details)
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
CXXFLAGS=-I. -I.. -I../comp -I../parser $(CCFLAGS)

SRC=	builtin.cpp std.cpp file.cpp math.cpp extra.cpp
OBJ=	builtin.o std.o file.o math.o extra.o

all:
	@echo Please run make from the src directory.

dgd:	table.o $(OBJ)
	@for i in table.o $(OBJ); do echo kfun/$$i; done > dgd

clean:
	rm -f dgd table.o $(OBJ)


$(OBJ): ../dgd.h ../config.h ../host.h ../alloc.h ../error.h ../str.h ../array.h
$(OBJ): ../object.h ../hash.h ../swap.h ../xfloat.h ../interpret.h ../data.h
std.o file.o: ../path.h ../editor.h
std.o: ../comm.h ../call_out.h
extra.o: ../asn.h
table.o: ../ext.h

std.o: ../comp/node.h ../comp/control.h ../comp/compile.h
table.o: ../comp/control.h

extra.o: ../parser/parse.h

$(OBJ): kfun.h table.h
table.o: $(OBJ)
