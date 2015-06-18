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
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)

SRC=	macro.c ppstr.c token.c special.c ppcontrol.c
OBJ=	macro.o ppstr.o token.o special.o ppcontrol.o

all:
	@echo Please run make from the src directory.

dgd:	$(OBJ)
	@for i in $(OBJ); do echo lex/$$i; done > dgd

clean:
	rm -f dgd $(OBJ)


$(OBJ): lex.h ../config.h ../host.h ../alloc.h ../str.h ../xfloat.h
macro.o special.o token.o ppcontrol.o: ../hash.h
token.o ppcontrol.o: ../path.h

$(OBJ): ../comp/node.h ../comp/compile.h ../comp/parser.h

$(OBJ): lex.h
macro.o special.o token.o ppcontrol.o: macro.h
ppstr.o token.o ppcontrol.o: ppstr.h
special.o token.o ppcontrol.o: special.h
token.o ppcontrol.o: token.h
ppcontrol.o: ppcontrol.h
