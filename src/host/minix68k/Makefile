#
# Makefile for host dependent object files for Minix 68K
#
HOST=	SUN
DEFINES=-D$(HOST)
DEBUG=	-g
CCFLAGS=-O $(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
CC=	gcc
AR=	ar
MAKE=	make

host.a:
	cd unix; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' host
	$(AR) cr $@ unix/*.o
	ranlib $@


clean:
	rm -f host.a
	cd unix; $(MAKE) clean
