#
# Makefile for host dependent object files for Minix 68K
#
HOST=	MINIX_68K
DEFINES=-D$(HOST)
DEBUG=	-DDEBUG
CCFLAGS=-O $(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
CC=	cc

SRC=	local.c lrand.c path.c rename.c dirent.c time.c alarm.c connect.c \
	xfloat.c
OBJ=	local.o lrand.o path.o rename.o dirent.o time.o alarm.o connect.o \
	xfloat.o crypt.o
SUBOBJ=	local.o lrand.o path.o rename.o dirent.o time.o xfloat.o crypt.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo host/$$i; done > dgd

sub:	$(SUBOBJ)
	@for i in $(SUBOBJ); do echo ../host/$$i; done > sub

clean:
	rm -f dgd sub $(SRC) $(OBJ)


local.c connect.c: minix68k/$@
	cp minix68k/$@ $@

lrand.c path.c rename.c dirent.c time.c alarm.c: unix/$@
	cp unix/$@ $@

xfloat.c: simfloat.c
	cp simfloat.c xfloat.c

$(OBJ):	../dgd.h ../host.h ../config.h ../alloc.h ../error.h
path.o: ../path.h
connect.o: ../str.h ../array.h ../object.h ../comm.h
xfloat.o: ../xfloat.h
