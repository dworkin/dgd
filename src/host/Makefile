#
# Makefile for host dependent object files for SunOS 4.x
#
HOST=	SUNOS4
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
CC=	gcc

SRC=	local.c random.c path.c dirent.c time.c alarm.c connect.c
OBJ=	local.o random.o path.o dirent.o time.o alarm.o connect.o
SUBOBJ=	local.o random.o path.o dirent.o time.o

dgd:	$(OBJ)
	@for i in $(OBJ); do echo host/$$i; done > dgd

sub:	$(SUBOBJ)
	@for i in $(SUBOBJ); do echo ../host/$$i; done > sub

clean:
	rm -f dgd sub $(SRC) $(OBJ)


$(SRC):	unix/$@
	cp unix/$@ $@

$(OBJ):	../dgd.h ../host.h
