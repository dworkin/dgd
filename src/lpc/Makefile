#
# Makefile for LPC objects
#
HOST=	NETBSD
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)
LIBS=
LINTFLAGS=-abcehpruz
CC=	gcc
DMAKE=	make
PRECOMP=../comp/a.out
CONFIG=	../../mud.dgd
MUDLIB=	../../mud

#SRC=	driver.c auto.c objregd.c rsrcd.c accessd.c userd.c \
#	api_objreg.c api_rsrc.c api_access.c api_user.c \
#	lib_connection.c lib_user.c lib_wiztool.c \
#	rsrc.c telnet.c binary.c user.c wiztool.c # lib_port.c port.c
SRC=

#OBJ=	driver.o auto.o objregd.o rsrcd.o accessd.o userd.o \
#	api_objreg.o api_rsrc.o api_access.o api_user.o \
#	lib_connection.o lib_user.o lib_wiztool.o \
#	rsrc.o telnet.o binary.o user.o wiztool.o # lib_port.o port.o
OBJ=

dgd:	$(OBJ) lpc.o
	@for i in $(OBJ) lpc.o; do echo lpc/$$i; done > dgd

lint:
	lint $(LINTFLAGS) $(CFLAGS) lpc.c

#$(SRC):	$(PRECOMP)

$(PRECOMP):
	cd ../comp; $(DMAKE) 'DMAKE=$(DMAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' \
			     'LIBS=$(LIBS)' a.out

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

lib_port.c: $(MUDLIB)/kernel/lib/port.c auto.c
	$(PRECOMP) $(CONFIG) /kernel/lib/port.c $@

port.c: $(MUDLIB)/kernel/obj/port.c auto.c lib_port.c
	$(PRECOMP) $(CONFIG) /kernel/obj/port.c $@

clean:
	rm -f dgd $(SRC) $(OBJ) lpc.o


$(OBJ) lpc.o: ../dgd.h ../config.h ../host.h ../error.h ../alloc.h ../str.h
$(OBJ) lpc.o: ../array.h ../object.h ../xfloat.h ../interpret.h ../data.h

$(OBJ) lpc.o: ../comp/csupport.h

lpc.o:	lpc.c list $(OBJ)
