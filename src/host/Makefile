#
# Makefile for host dependent object files for Minix 68K
#
HOST=	SUN
DEFINES=-D$(HOST)
DEBUG=	-g
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
CC=	cc
AR=	ar cr
MAKE=	make

host.a:
	cd unix; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' host
	$(AR) $@ unix/*.o; ranlib $@


clean:
	rm -f host.a
	cd unix; $(MAKE) clean
