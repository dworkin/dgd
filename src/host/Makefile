#
# Makefile for host dependent object files for SunOS 4.x
#
HOST=	SUN
DEFINES=-D$(HOST)
DEBUG=	-g -DDEBUG
CCFLAGS=$(DEFINES) $(DEBUG)
CFLAGS=	-I. -I.. $(CCFLAGS)
CC=	gcc
AR=	ar cr
MAKE=	make

host.a:
	cd unix; $(MAKE) 'MAKE=$(MAKE)' 'CC=$(CC)' 'CCFLAGS=$(CCFLAGS)' host
	$(AR) $@ unix/*.o; ranlib $@

clean:
	rm -f host.a
	cd unix; $(MAKE) clean
