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
CFLAGS=	-I. -I.. $(CCFLAGS)

SRC=	line.c buffer.c fileio.c regexp.c vars.c edcmd.c cmdsub.c
OBJ=	line.o buffer.o fileio.o regexp.o vars.o edcmd.o cmdsub.o

all:
	@echo Please run make from the src directory.

dgd:	$(OBJ)
	@for i in $(OBJ); do echo ed/$$i; done > dgd

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
