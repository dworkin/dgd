#
# Makefile for host dependent object files for Minix 68K
#
HOST=	MINIX_68K
DEFINES=-D$(HOST)
DEBUG=	-DDEBUG
CCFLAGS=-O $(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
CC=	cc
AR=	ar cr
MAKE=	make

host.a:
	cd unix; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' host
	$(AR) $@ unix/*.o;

clean:
	rm -f host.a
	cd unix; $(MAKE) clean
