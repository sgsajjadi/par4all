#
# $Id$
#
# files:

LIB_CFILES=	debug.c \
		file.c \
		message.c \
		dotting.c \
		args.c \
		system.c \
		exception.c \
		mem_spy.c \
		perf_spy.c \
		malloc_debug.c 

LIB_HEADERS=	misc-local.h

LIB_OBJECTS=	$(LIB_CFILES:.c=.o)

