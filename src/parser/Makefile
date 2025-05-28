#
# This file is part of DGD, https://github.com/dworkin/dgd
# Copyright (C) 1993-2010 Dworkin B.V.
# Copyright (C) 2010-2025 DGD Authors (see the commit log for details)
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
CXXFLAGS=-I. -I.. $(CCFLAGS)

SRC=	grammar.cpp dfa.cpp srp.cpp parse.cpp
OBJ=	grammar.o dfa.o srp.o parse.o

all:
	@echo Please run make from the src directory.

dgd:	$(OBJ)
	@for i in $(OBJ); do echo parser/$$i; done > dgd

clean:
	rm -f dgd $(OBJ)


$(OBJ): ../dgd.h ../config.h ../host.h ../alloc.h ../error.h
grammar.o dfa.o: ../hash.h ../str.h
parse.o: ../str.h ../array.h ../object.h ../xfloat.h ../hash.h ../swap.h
parse.o: ../interpret.h ../data.h

grammar.o parse.o: grammar.h
dfa.o parse.o: dfa.h
srp.o parse.o: srp.h
parse.o: parse.h
