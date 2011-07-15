#
# This file is part of DGD, http://dgd-osr.sourceforge.net/
# Copyright (C) 2011 DGD Authors (see the file Changelog for details)
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
CFLAGS=	-I.. $(CCFLAGS)
LDFLAGS=
LIBS=
LINTFLAGS=-abcehpruz
CC=	gcc
LD=	$(CC)
DMAKE=	make
BIN=	../../bin

SRC=	login.o relay.o
OBJ=	login.o relay.o

a.out:	$(OBJ)
	$(LD) $(DEBUG) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

$(BIN)/relay: a.out
	-mv $(BIN)/relay $(BIN)/relay.old
	cp a.out $(BIN)/relay

install: $(BIN)/relay

lint:
	lint $(LINTFLAGS) $(CFLAGS) $(SRC)

clean:
	rm -f a.out $(OBJ)


$(OBJ): ../dgd.h ../host.h ../config.h ../alloc.h ../error.h

$(OBJ): login.h
