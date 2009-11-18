#
# $Header: /home/ben/.cvs_rep/nds/Debug/debug_lib_src/tcp_comms/Makefile,v 1.2 2006/10/10 12:11:48 ben Exp $
#
# The Makefile for the Native NDS version
#
# This makefile builds a library of the wmbhost common code and the .nds
# file. The DS specific code is built by makefiles in the nds directory.
#

NDSTOOL = ndstool

PREFIX = arm-eabi-
CC = $(PREFIX)gcc
LD = $(PREFIX)ld
AR = $(PREFIX)ar
AS = $(PREFIX)as
OBJCOPY	= $(PREFIX)objcopy

ARCH = -mthumb-interwork -march=armv5te -mtune=arm946e-s

INC_DIRS = $(DEVKITPRO)/libnds/include $(DEVKITPRO)/libnds/wifi/include ..

CFLAGS	= -g -W -Wall -O2 -DARM9\
	 -fomit-frame-pointer\
	-ffast-math\
	$(ARCH) $(addprefix -I,$(INC_DIRS))

ASFLAGS = -g $(ARCH)

# The tcp comms library
tcp_lib_name_base = debugtcp9
tcp_lib_name = lib$(tcp_lib_name_base).a

tcp_sources = debug_tcp.c
tcp_objects = $(tcp_sources:.c=.o)

.PHONY : all
all : $(tcp_lib_name)


# Make the libraries
$(tcp_lib_name): $(tcp_objects)
	$(AR) rcs $@ $^

# Make the object files from C source
# Add -DDO_LOGGING and rebuild the sources to enable logging.
%.o : %.c
	@echo building $@ from $<
	$(CC) $(CFLAGS) $< -c -o $@

#	$(CC) $(CFLAGS) $< -DDO_LOGGING -c -o $@



.PHONY : clean
clean :
	-rm -f *.o
	-rm -f $(tcp_lib_name)

# Generate the dependencies
%.d: %.c
	set -e; $(CC) -MM $(CFLAGS) $< \
		| sed 's|\($*\)\.o[ :]*|\1.o $@ : |g' > $@; \
		[ -s $@ ] || rm -f $@

# include the dependencies files
include $(tcp_sources:.c=.d)
