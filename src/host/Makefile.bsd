#
# Makefile for host dependent object files for BSD
#
HOST=	GENERIC_BSD
DEFINES=-D$(HOST)
DEBUG=
CCFLAGS=-O2 $(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
CC=	gcc

SRC=	local.c random.c dirent.c time.c alarm.c connect.c xfloat.c
OBJ=	local.o random.o dirent.o time.o alarm.o connect.o crypt.o \
	xfloat.o
SUBOBJ=	local.o random.o dirent.o time.o crypt.o xfloat.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo host/$$i; done > dgd

sub:	$(SUBOBJ)
	@for i in $(SUBOBJ); do echo ../host/$$i; done > sub

clean:
	rm -f dgd sub $(SRC) $(OBJ)


local.c random.c dirent.c time.c alarm.c connect.c: unix/$@
	cp unix/$@ $@

xfloat.c: simfloat.c
	cp simfloat.c xfloat.c

$(OBJ):	../dgd.h ../host.h ../config.h ../alloc.h ../error.h
connect.o: ../str.h ../array.h ../object.h ../comm.h
xfloat.o: ../xfloat.h
