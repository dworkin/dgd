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
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)
LDFLAGS=
LIBS=
LINTFLAGS=-abcehpruz
CC=	gcc
LD=	$(CC)

SRC=	macro.c ppstr.c token.c special.c ppcontrol.c
OBJ=	macro.o ppstr.o token.o special.o ppcontrol.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo lex/$$i; done > dgd

comp:	$(OBJ)
	@for i in $(OBJ); do echo ../lex/$$i; done > comp

lint:
	lint $(LINTFLAGS) $(CFLAGS) $(SRC)

clean:
	rm -f dgd comp $(OBJ)


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
