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
LDFLAGS=
LIBS=
LINTFLAGS=-abcehpruz
CC=	gcc
LD=	$(CC)

SRC=	line.c buffer.c fileio.c regexp.c vars.c edcmd.c cmdsub.c
OBJ=	line.o buffer.o fileio.o regexp.o vars.o edcmd.o cmdsub.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo ed/$$i; done > dgd

lint:
	lint $(LINTFLAGS) $(CFLAGS) $(SRC)

clean:
	rm -f dgd $(OBJ)


$(OBJ): ../config.h ../host.h ../alloc.h ../error.h
fileio.o: ../path.h

$(OBJ): ed.h
buffer.o fileio.o edcmd.o cmdsub.o: buffer.h
edcmd.o cmdsub.o: edcmd.h
cmdsub.o fileio.o: fileio.h
line.o buffer.o fileio.o edcmd.o cmdsub.o: line.h
regexp.o edcmd.o cmdsub.o: regexp.h
vars.o edcmd.o cmdsub.o: vars.h
