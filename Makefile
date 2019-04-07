#
# Quick 'n dirty unix Makefile
#
# Mike Oliphant (oliphant@gtk.org)
#

CC=     gcc

CFLAGS= -Wall -O3 -DHAVE_MEMCPY

ifneq ($(OSTYPE),beos)
INSTALL_PATH= /usr/local/bin
else
INSTALL_PATH= $(HOME)/config/bin
endif

# BeOS doesn't have libm (it's all in libroot)
ifneq ($(OSTYPE),beos)
LIBS= -lm
else
# BeOS: without this it wants to use bcopy() :^)
CFLAGS+= -DHAVE_MEMCPY
endif

OBJS=	mp3gain.o apetag.o id3tag.o gain_analysis.o rg_error.o \
	mpglibDBL/common.o mpglibDBL/dct64_i386.o \
	mpglibDBL/decode_i386.o mpglibDBL/interface.o \
	mpglibDBL/layer3.o mpglibDBL/tabinit.o

all: mp3gain

mp3gain: $(OBJS)
	$(CC) -o mp3gain $(OBJS) $(LIBS)
ifeq ($(OSTYPE),beos)
	mimeset -f mp3gain
endif

install:
	cp -p mp3gain "$(INSTALL_PATH)"
ifeq ($(OSTYPE),beos)
	mimeset -f "$(INSTALL_PATH)/mp3gain"
endif

clean: 
	-rm -rf mp3gain *.o mpglibDBL/*.o
