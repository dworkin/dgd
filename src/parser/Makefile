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
CFLAGS=	-I. -I.. $(CCFLAGS)
LINTFLAGS=-abcehpruz
CC=	gcc

SRC=	grammar.c dfa.c srp.c parse.c
OBJ=	grammar.o dfa.o srp.o parse.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo parser/$$i; done > dgd

comp:	$(OBJ)
	@for i in $(OBJ); do echo ../parser/$$i; done > comp

lint:
	lint $(LINTFLAGS) $(CFLAGS) $(SRC)

clean:
	rm -f dgd comp $(OBJ)


$(OBJ): ../dgd.h ../config.h ../host.h ../alloc.h ../error.h
grammar.o dfa.o: ../hash.h ../str.h
parse.o: ../str.h ../array.h ../object.h ../interpret.h ../data.h

grammar.o parse.o: grammar.h
dfa.o parse.o: dfa.h
srp.o parse.o: srp.h
parse.o: parse.h
