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
OS=$(shell uname -s)
ifeq ($(OS),Darwin)
  HOST=DARWIN
endif
ifeq ($(OS),Linux)
  HOST=LINUX
endif
ifeq ($(OS),FreeBSD)
  HOST=FREEBSD
endif
ifeq ($(OS),NetBSD)
  HOST=FREEBSD
endif
ifeq ($(OS),OpenBSD)
  HOST=FREEBSD
endif
ifeq ($(OS),SunOS)
  HOST=SOLARIS
endif
ifndef HOST
  $(error HOST is undefined)
endif

DEFINES=		# -DLARGENUM -DSLASHSLASH -DNOFLOAT -DCLOSURES
DEBUG=	-g -DDEBUG
CCFLAGS=-D$(HOST) $(DEFINES) $(DEBUG)
CXXFLAGS=-I. -Icomp -Ilex -Ied -Iparser -Ikfun $(CCFLAGS)
LDFLAGS=
LIBS=	-ldl
CXX=	c++
LD=	$(CXX)
YACC=	yacc
BIN=	../bin

ifeq ($(HOST),LINUX)
  DEFINES+=-D_FILE_OFFSET_BITS=64
  CXX+=-pthread
endif
ifeq ($(HOST),FREEBSD)
  LIBS=
  CXX+=-pthread
endif
ifeq ($(HOST),SOLARIS)
  DEFINES+=-D_FILE_OFFSET_BITS=64
  LIBS+=-lsocket -lnsl
  CXX=CC -mt -std=c++11
endif

SRC=	alloc.cpp error.cpp hash.cpp swap.cpp str.cpp array.cpp object.cpp \
	data.cpp path.cpp editor.cpp comm.cpp call_out.cpp interpret.cpp \
	config.cpp ext.cpp dgd.cpp
OBJ=	alloc.o error.o hash.o swap.o str.o array.o object.o data.o path.o \
	editor.o comm.o call_out.o interpret.o config.o ext.o dgd.o

a.out:	$(OBJ) comp/dgd lex/dgd ed/dgd parser/dgd kfun/dgd host/dgd
	$(LD) $(DEBUG) $(LDFLAGS) -o $@ $(OBJ) `cat comp/dgd` `cat lex/dgd` \
	      `cat ed/dgd` `cat parser/dgd` `cat kfun/dgd` `cat host/dgd` \
	      $(LIBS)

comp/dgd::
	$(MAKE) -C comp 'CXX=$(CXX)' 'CCFLAGS=$(CCFLAGS)' 'YACC=$(YACC)' dgd

lex/dgd::
	$(MAKE) -C lex 'CXX=$(CXX)' 'CCFLAGS=$(CCFLAGS)' dgd

ed/dgd::
	$(MAKE) -C ed 'CXX=$(CXX)' 'CCFLAGS=$(CCFLAGS)' dgd

parser/dgd::
	$(MAKE) -C parser 'CXX=$(CXX)' 'CCFLAGS=$(CCFLAGS)' dgd

kfun/dgd::
	$(MAKE) -C kfun 'CXX=$(CXX)' 'CCFLAGS=$(CCFLAGS)' dgd


host/dgd::
	$(MAKE) -C host 'CXX=$(CXX)' 'HOST=$(HOST)' 'CCFLAGS=$(CCFLAGS)' dgd

all:	a.out

$(BIN)/dgd: a.out
	-mv $(BIN)/dgd $(BIN)/dgd.old
	cp a.out $(BIN)/dgd

install: $(BIN)/dgd

comp/parser.h: comp/parser.y
	$(MAKE) -C comp 'YACC=$(YACC)' parser.h

ed/ed::
	$(MAKE) -C ed 'CXX=$(CXX)' 'HOST=$(HOST)' 'CCFLAGS=$(CCFLAGS)' \
		'LD=$(LD)' 'LDFLAGS=$(LDFLAGS)' ed

lex/lex::
	$(MAKE) -C lex 'CXX=$(CXX)' 'HOST=$(HOST)' 'CCFLAGS=$(CCFLAGS)' \
		'LD=$(LD)' 'LDFLAGS=$(LDFLAGS)' lex

clean:
	rm -f a.out $(OBJ)
	$(MAKE) -C comp clean
	$(MAKE) -C lex clean
	$(MAKE) -C ed clean
	$(MAKE) -C parser clean
	$(MAKE) -C kfun clean
	$(MAKE) -C host 'HOST=$(HOST)' clean


path.o config.o dgd.o: comp/node.h comp/compile.h
config.o: comp/parser.h
array.o object.o data.o config.o interpret.o ext.o: comp/control.h

path.o config.o: lex/ppcontrol.h

editor.o: ed/edcmd.h

data.o: parser/parse.h

interpret.o config.o ext.o: kfun/table.h

$(OBJ):	dgd.h config.h host.h alloc.h error.h
error.o str.o array.o object.o data.o: str.h array.h object.h hash.h swap.h
path.o comm.o editor.o call_out.o: str.h array.h object.h hash.h swap.h
interpret.o config.o ext.o dgd.o: str.h array.h object.h hash.h swap.h
array.o error.o str.o object.o data.o comm.o call_out.o interpret.o: xfloat.h
path.o config.o ext.o dgd.o: xfloat.h
error.o array.o object.o data.o path.o editor.o comm.o: interpret.h
call_out.o interpret.o config.o ext.o dgd.o: interpret.h
error.o str.o array.o object.o data.o path.o comm.o call_out.o: data.h
interpret.o config.o ext.o dgd.o: data.h
path.o config.o: path.h
hash.o: hash.h
swap.o: hash.h swap.h
editor.o config.o dgd.o: editor.h
data.o call_out.o config.o dgd.o: call_out.h
error.o comm.o config.o ext.o dgd.o: comm.h
object.o data.o interpret.o config.o dgd.o: ext.h
comm.o config.o: version.h
