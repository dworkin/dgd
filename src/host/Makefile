#
# Makefile for host dependent object files for NetBSD
#
HOST=	DARWIN
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
LINTFLAGS=-abcehpruz
CC=	gcc

SRC=	local.c random.c dirent.c time.c connect.c xfloat.c
OBJ=	local.o random.o dirent.o time.o connect.o crypt.o xfloat.o asn.o
SUBOBJ=	local.o random.o dirent.o time.o crypt.o xfloat.o asn.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo host/$$i; done > dgd

lint:
	lint $(LINTFLAGS) $(CFLAGS) $(SRC) crypt.c

sub:	$(SUBOBJ)
	@for i in $(SUBOBJ); do echo ../host/$$i; done > sub

clean:
	rm -f dgd sub $(SRC) $(OBJ)


local.c: unix/local.c
	cp unix/$@ $@

random.c: unix/random.c
	cp unix/$@ $@

dirent.c: unix/dirent.c
	cp unix/$@ $@

time.c: unix/time.c
	cp unix/$@ $@

connect.c: unix/connect.c
	cp unix/$@ $@

xfloat.c: simfloat.c
	cp simfloat.c $@

$(OBJ):	../dgd.h ../host.h ../config.h ../alloc.h ../error.h
connect.o: ../hash.h ../comm.h
xfloat.o: ../xfloat.h
