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
LIBS=
LINTFLAGS=-abcehpruz
CC=	gcc
DMAKE=	make
YACC=	yacc
PRECOMP=../comp/a.out
CONFIG=	../../mud.dgd
MUDLIB=	../../mud

#SRC=	driver.c auto.c objregd.c rsrcd.c accessd.c userd.c \
#	api_objreg.c api_rsrc.c api_access.c api_user.c api_tls.c \
#	lib_connection.c lib_user.c lib_wiztool.c \
#	rsrc.c telnet.c binary.c user.c wiztool.c
SRC=

#OBJ=	driver.o auto.o objregd.o rsrcd.o accessd.o userd.o \
#	api_objreg.o api_rsrc.o api_access.o api_user.o api_tls.o \
#	lib_connection.o lib_user.o lib_wiztool.o \
#	rsrc.o telnet.o binary.o user.o wiztool.o
OBJ=

dgd:	$(OBJ) lpc.o
	@for i in $(OBJ) lpc.o; do echo lpc/$$i; done > dgd

lint:
	lint $(LINTFLAGS) $(CFLAGS) lpc.c

#$(SRC):	$(PRECOMP)

$(PRECOMP):
	cd ../comp; $(DMAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' \
			     'YACC=$(YACC)' 'LIBS=$(LIBS)' a.out

driver.c: $(MUDLIB)/kernel/sys/driver.c
	$(PRECOMP) $(CONFIG) /kernel/sys/driver.c $@

auto.c:	$(MUDLIB)/kernel/lib/auto.c
	$(PRECOMP) $(CONFIG) /kernel/lib/auto.c $@

objregd.c: $(MUDLIB)/kernel/sys/objregd.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/sys/objregd.c $@

rsrcd.c: $(MUDLIB)/kernel/sys/rsrcd.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/sys/rsrcd.c $@

accessd.c: $(MUDLIB)/kernel/sys/accessd.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/sys/accessd.c $@

userd.c: $(MUDLIB)/kernel/sys/userd.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/sys/userd.c $@

api_objreg.c: $(MUDLIB)/kernel/lib/api/objreg.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/lib/api/objreg.c $@

api_rsrc.c: $(MUDLIB)/kernel/lib/api/rsrc.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/lib/api/rsrc.c $@

api_access.c: $(MUDLIB)/kernel/lib/api/access.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/lib/api/access.c $@

api_user.c: $(MUDLIB)/kernel/lib/api/user.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/lib/api/user.c $@

api_tls.c: $(MUDLIB)/kernel/lib/api/tls.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/lib/api/tls.c $@

lib_connection.c: $(MUDLIB)/kernel/lib/connection.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/lib/connection.c $@

lib_user.c: $(MUDLIB)/kernel/lib/user.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/lib/user.c $@

lib_wiztool.c: $(MUDLIB)/kernel/lib/wiztool.c auto.c \
	       api_rsrc.c api_access.c api_user.c
	$(PRECOMP) $(CONFIG) /kernel/lib/wiztool.c $@

rsrc.c: $(MUDLIB)/kernel/obj/rsrc.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/obj/rsrc.c $@

telnet.c: $(MUDLIB)/kernel/obj/telnet.c auto.c lib_connection.c
	$(PRECOMP) $(CONFIG) /kernel/obj/telnet.c $@

binary.c: $(MUDLIB)/kernel/obj/binary.c auto.c lib_connection.c
	$(PRECOMP) $(CONFIG) /kernel/obj/binary.c $@

user.c: $(MUDLIB)/kernel/obj/user.c auto.c lib_user.c api_rsrc.c api_user.c
	$(PRECOMP) $(CONFIG) /kernel/obj/user.c $@

wiztool.c: $(MUDLIB)/kernel/obj/wiztool.c auto.c lib_wiztool.c
	$(PRECOMP) $(CONFIG) /kernel/obj/wiztool.c $@

clean:
	rm -f dgd $(SRC) $(OBJ) lpc.o


$(OBJ) lpc.o: ../dgd.h ../config.h ../host.h ../error.h ../alloc.h ../str.h
$(OBJ) lpc.o: ../array.h ../object.h ../xfloat.h ../interpret.h ../data.h

$(OBJ) lpc.o: ../comp/csupport.h

lpc.o:	lpc.c list $(OBJ)
