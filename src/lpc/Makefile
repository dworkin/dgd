#
# Makefile for LPC objects
#
HOST=	SUN
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. -I../comp $(CCFLAGS)
CC=	gcc
MAKE=	make
PRECOMP=../comp/a.out
CONFIG=	../lpmud
MUDLIB=	../../2.4.5

SRC=	auto.c driver.c global.c user.c editor.c cindent.c
OBJ=	auto.o driver.o global.o user.o editor.o cindent.o

auto.c:	$(MUDLIB)/dgd/lib/auto.c
	$(PRECOMP) $(CONFIG) /dgd/lib/auto.c > auto.c

driver.c: $(MUDLIB)/dgd/sys/driver.c
	$(PRECOMP) $(CONFIG) /dgd/sys/driver.c > driver.c

global.c: $(MUDLIB)/dgd/sys/global.c auto.c
	$(PRECOMP) $(CONFIG) /dgd/sys/global.c > global.c

user.c:	$(MUDLIB)/dgd/lib/user.c auto.c
	$(PRECOMP) $(CONFIG) /dgd/lib/user.c > user.c

editor.c: $(MUDLIB)/dgd/std/editor.c auto.c
	$(PRECOMP) $(CONFIG) /dgd/std/editor.c > editor.c

cindent.c: $(MUDLIB)/dgd/std/cindent.c auto.c
	$(PRECOMP) $(CONFIG) /dgd/std/cindent.c > cindent.c

$(SRC):	$(PRECOMP)

$(PRECOMP):
	cd ../comp; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' a.out

dgd:	$(OBJ) lpc.o
	@for i in $(OBJ) lpc.o; do echo lpc/$$i; done > dgd

clean:
	rm -f dgd $(SRC) $(OBJ) lpc.o


$(OBJ) lpc.o: ../dgd.h ../config.h ../host.h ../error.h ../alloc.h ../str.h
$(OBJ) lpc.o: ../array.h ../object.h ../interpret.h ../data.h

$(OBJ) lpc.o: ../comp/csupport.h

lpc.o:	$(SRC)
