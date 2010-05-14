# $Id$
#
# Copyright 1989-2010 MINES ParisTech
#
# This file is part of PIPS.
#
# PIPS is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# PIPS is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.
#
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with PIPS.  If not, see <http://www.gnu.org/licenses/>.
#



# PIPS Libraries
PIPSLIBS_LIBS = ../../../../../Libs/
LIBS_PIPS =  $(PIPSLIBS_LIBS)/ri-util/.libs/libri-util.a \
	$(PIPSLIBS_LIBS)/text-util/.libs/libtext-util.a \
	$(PIPSLIBS_LIBS)/syntax/.libs/libsyntax.a \
	$(PIPSLIBS_LIBS)/alias-classes/.libs/libalias-classes.a \
	$(PIPSLIBS_LIBS)/ri-util/.libs/libri-util.a \
	$(PIPSLIBS_LIBS)/misc/.libs/libmisc.a \
	$(PIPSLIBS_LIBS)/newgen/.libs/libnewgen.a

GFC2PIPS_OBJS = fortran/gfc2pips.o fortran/gfc2pips-util.o fortran/gfc2pips-stubs.o

# The compiler itself is called f951.
f951$(exeext): $(F95_OBJS) $(LIBS_PIPS) $(GFC2PIPS_OBJS) \
		$(BACKEND) $(LIBDEPS) attribs.o
	$(CC) $(ALL_CFLAGS) $(LDFLAGS) -o $@ \
		$(F95_OBJS) $(BACKEND) $(LIBS) $(GFC2PIPS_OBJS) $(LIBS_PIPS) $(LINEARLIBS_LIBS) $(NEWGENLIBS_LIBS)  attribs.o $(BACKENDLIBS) -lgmp -lmpfr

#INCLUDES += -I$(NEWGEN_ROOT)/include/ -I$(PIPS_ROOT)/include/ -I $(LINEAR_ROOT)/include/
PIPS_INC_PRE   = -I../../../../../Documentation/newgen/ 
PIPS_INC_POST  = -I../../../../../Libs/preprocessor/
PIPS_INC_POST += -I../../../../../Libs/ri-util/
PIPS_INC_POST += -I../../../../../Libs/syntax/
PIPS_INC_POST += -I../../../../../Libs/misc/
PIPS_INC_POST += -I../../../../../Libs/newgen/
PIPS_INC_POST += -I../../../$(pipssrcdir)/../../Documentation/newgen/
PIPS_INC_POST += -I../../../$(pipssrcdir)/../../Documentation/constants/
PIPS_INC_POST += $(LINEARLIBS_CFLAGS) $(NEWGENLIBS_CFLAGS) 



fortran/gfc2pips-stubs.o: fortran/gfc2pips-stubs.c fortran/gfc2pips.h fortran/gfc2pips-private.h
	$(CC) $(PIPS_INC_PRE) -std=c99 -g -c $(ALL_CPPFLAGS) $(PIPS_INC_POST) -DBASEVER=$(BASEVER_s)  \
		$< $(OUTPUT_OPTION)

fortran/gfc2pips-util.o: fortran/gfc2pips-util.c fortran/gfc2pips.h fortran/gfc2pips-private.h
	$(CC) $(PIPS_INC_PRE) -std=c99 -g -c $(ALL_CPPFLAGS) $(PIPS_INC_POST) -DBASEVER=$(BASEVER_s)  \
		$< $(OUTPUT_OPTION)

fortran/gfc2pips.o: fortran/gfc2pips.c fortran/gfc2pips.h fortran/gfc2pips-private.h
	$(CC) $(PIPS_INC_PRE) -std=c99 -g -c $(ALL_CPPFLAGS) $(PIPS_INC_POST) -DBASEVER=$(BASEVER_s)  \
		$< $(OUTPUT_OPTION)

