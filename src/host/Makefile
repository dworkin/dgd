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

ifeq ($(HOST),LINUX)
  SYSV_STYLE=1
endif
ifeq ($(HOST),SOLARIS)
  SYSV_STYLE=1
endif
ifeq ($(HOST),DECALPHA)
  SYSV_STYLE=1
endif
ifeq ($(HOST),GENERIC_SYSV)
  SYSV_STYLE=1
endif

SRC=	local.cpp dirent.cpp dload.cpp time.cpp connect.cpp
OBJ=	local.o dirent.o dload.o time.o connect.o crypt.o asn.o
ifdef SIMFLOAT
  OBJ+=simfloat.o
else
  OBJ+=hostfloat.o
endif
ifdef SYSV_STYLE
  SRC+=lrand48.cpp
  OBJ+=lrand48.o
else
  SRC+=random.cpp
  OBJ+=random.o
endif

all:
	@echo Please run make from the src directory.

dgd:	$(OBJ)
	@for i in $(OBJ); do echo host/$$i; done > dgd

clean:
	rm -f dgd $(SRC) $(OBJ)


local.cpp: unix/local.cpp
	cp unix/$@ $@

random.cpp: unix/random.cpp
	cp unix/$@ $@

lrand48.cpp: unix/lrand48.cpp
	cp unix/$@ $@

dirent.cpp: unix/dirent.cpp
	cp unix/$@ $@

dload.cpp: unix/dload.cpp
	cp unix/$@ $@

time.cpp: unix/time.cpp
	cp unix/$@ $@

connect.cpp: unix/connect.cpp
	cp unix/$@ $@

$(OBJ):	../dgd.h ../host.h ../config.h ../alloc.h ../error.h
connect.o: ../hash.h ../comm.h
simfloat.o hostfloat.o crypt.o asn.o: ../xfloat.h
hostfloat.o: ../ext.h
crypt.o asn.o: ../str.h ../array.h ../object.h ../hash.h ../swap.h
crypt.o asn.o: ../interpret.h ../data.h
asn.o: ../asn.h
