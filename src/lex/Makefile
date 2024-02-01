#
# This file is part of DGD, https://github.com/dworkin/dgd
# Copyright (C) 1993-2010 Dworkin B.V.
# Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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
CXXFLAGS=-I. -I.. -I../comp $(CCFLAGS)

SRC=	macro.cpp ppstr.cpp token.cpp special.cpp ppcontrol.cpp
OBJ=	macro.o ppstr.o token.o special.o ppcontrol.o

all:
	@echo Please run make from the src directory.

dgd:	$(OBJ)
	@for i in $(OBJ); do echo lex/$$i; done > dgd

clean:
	rm -f dgd $(OBJ)

lex:	$(OBJ) lex.o
	$(LD) $(DEBUG) $(LDFLAGS) -o $@ $(OBJ) lex.o


$(OBJ) lex.o : lex.h ../config.h ../host.h ../alloc.h ../xfloat.h
macro.o special.o token.o ppcontrol.o lex.o: ../hash.h
token.o ppcontrol.o lex.o: ../path.h

$(OBJ) lex.o: ../comp/node.h ../comp/compile.h ../comp/parser.h

$(OBJ) lex.o: lex.h
macro.o special.o token.o ppcontrol.o: macro.h
ppstr.o token.o ppcontrol.o: ppstr.h
special.o token.o ppcontrol.o: special.h token.h
token.o ppcontrol.o lex.o: ppcontrol.h
