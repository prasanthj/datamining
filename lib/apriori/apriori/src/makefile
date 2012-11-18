#-----------------------------------------------------------------------
# File    : makefile
# Contents: build apriori program (on Unix systems)
# Author  : Christian Borgelt
# History : 1995.??.?? file created
#           1997.10.13 macro ADDFLAGS added
#           1997.12.07 minor improvements
#           1998.01.04 table scanner management added
#           1999.11.11 vector operations module added
#           2000.11.04 modules arrays, symtab, and tabread made external
#           2001.11.18 module tract added (transaction management)
#           2003.12.12 preprocessor definition ALIGN8 added
#           2008.08.11 adapted to name change of vecops to arrays
#           2008.08.22 module escape added (for module tabread)
#           2010.10.08 changed standard from -ansi to -std=c99
#           2011.07.22 module ruleval added (rule evaluation)
#           2011.10.18 special program version apriacc added
#-----------------------------------------------------------------------
# For large file support (> 2GB) compile with
#   make ADDFLAGS=-D_FILE_OFFSET_BITS=64
#-----------------------------------------------------------------------
SHELL    = /bin/bash
THISDIR  = ../../apriori/src
UTILDIR  = ../../util/src
MATHDIR  = ../../math/src
TRACTDIR = ../../tract/src

CC       = gcc -std=c99
# CC       = g++
CFBASE   = -Wall -pedantic $(ADDFLAGS)
CFLAGS   = $(CFBASE) -DNDEBUG -O3
# CFLAGS   = $(CFBASE) -DNDEBUG -O3 -DALIGN8
# CFLAGS   = $(CFBASE) -DNDEBUG -O3 -DBENCH
# CFLAGS   = $(CFBASE) -g
# CFLAGS   = $(CFBASE) -g -DALIGN8
INCS     = -I$(UTILDIR) -I$(MATHDIR) -I$(TRACTDIR)

LD       = gcc
LDFLAGS  =
LIBS     = -lm

HDRS     = $(UTILDIR)/arrays.h  $(UTILDIR)/symtab.h  \
           $(UTILDIR)/escape.h  $(UTILDIR)/tabread.h \
           $(UTILDIR)/scanner.h $(MATHDIR)/gamma.h   \
           $(MATHDIR)/chi2.h    $(MATHDIR)/ruleval.h \
           $(TRACTDIR)/tract.h  $(TRACTDIR)/report.h \
           istree.h apriori.h
OBJS     = $(UTILDIR)/arrays.o  $(UTILDIR)/idmap.o   \
           $(UTILDIR)/escape.o  $(UTILDIR)/tabread.o \
           $(UTILDIR)/scform.o  $(MATHDIR)/gamma.o   \
           $(MATHDIR)/chi2.o    $(MATHDIR)/ruleval.o \
           $(TRACTDIR)/tatree.o $(TRACTDIR)/report.o \
           istree.o
PRGS     = apriori apriacc

#-----------------------------------------------------------------------
# Build Programs
#-----------------------------------------------------------------------
all:       $(PRGS)

apriori:   $(OBJS) apriori.o makefile
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) apriori.o -o $@

apriacc:   $(OBJS) apriacc.o makefile
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) apriacc.o -o $@

#-----------------------------------------------------------------------
# Main Programs
#-----------------------------------------------------------------------
apriori.o: $(HDRS)
apriori.o: apriori.c makefile
	$(CC) $(CFLAGS) $(INCS) -c apriori.c -o $@

apriacc.o: $(HDRS)
apriacc.o: apriori.c makefile
	$(CC) $(CFLAGS) $(INCS) -DAPRIACC -c apriori.c -o $@

#-----------------------------------------------------------------------
# Item Set Tree Management
#-----------------------------------------------------------------------
istree.o:  $(HDRS)
istree.o:  istree.c makefile
	$(CC) $(CFLAGS) $(INCS) -c istree.c -o $@

#-----------------------------------------------------------------------
# External Modules
#-----------------------------------------------------------------------
$(UTILDIR)/arrays.o:
	cd $(UTILDIR);  $(MAKE) arrays.o  ADDFLAGS=$(ADDFLAGS)
$(UTILDIR)/idmap.o:
	cd $(UTILDIR);  $(MAKE) idmap.o   ADDFLAGS=$(ADDFLAGS)
$(UTILDIR)/escape.o:
	cd $(UTILDIR);  $(MAKE) escape.o  ADDFLAGS=$(ADDFLAGS)
$(UTILDIR)/tabread.o:
	cd $(UTILDIR);  $(MAKE) tabread.o ADDFLAGS=$(ADDFLAGS)
$(UTILDIR)/scform.o:
	cd $(UTILDIR);  $(MAKE) scform.o  ADDFLAGS=$(ADDFLAGS)
$(UTILDIR)/storage.o:
	cd $(UTILDIR);  $(MAKE) storage.o ADDFLAGS=$(ADDFLAGS)
$(MATHDIR)/gamma.o:
	cd $(MATHDIR);  $(MAKE) gamma.o   ADDFLAGS=$(ADDFLAGS)
$(MATHDIR)/chi2.o:
	cd $(MATHDIR);  $(MAKE) chi2.o    ADDFLAGS=$(ADDFLAGS)
$(MATHDIR)/ruleval.o:
	cd $(MATHDIR);  $(MAKE) ruleval.o ADDFLAGS=$(ADDFLAGS)
$(TRACTDIR)/tatree.o:
	cd $(TRACTDIR); $(MAKE) tatree.o  ADDFLAGS=$(ADDFLAGS)
$(TRACTDIR)/report.o:
	cd $(TRACTDIR); $(MAKE) report.o  ADDFLAGS=$(ADDFLAGS)

#-----------------------------------------------------------------------
# Source Distribution Packages
#-----------------------------------------------------------------------
dist:
	$(MAKE) clean
	cd ../..; rm -f apriori.zip apriori.tar.gz; \
        zip -rq apriori.zip apriori/{src,ex,doc} \
          tract/src/{tract.[ch],report.[ch]} \
          tract/src/{makefile,tract.mak} tract/doc \
          math/src/{gamma.[ch],chi2.[ch],ruleval.[ch]} \
          math/src/{makefile,math.mak} math/doc \
          util/src/{fntypes.h,error.h} \
          util/src/{arrays.[ch],escape.[ch],symtab.[ch]} \
          util/src/{tabread.[ch],scanner.[ch]} \
          util/src/{makefile,util.mak} util/doc; \
        tar cfz apriori.tar.gz apriori/{src,ex,doc} \
          tract/src/{tract.[ch],report.[ch]} \
          tract/src/{makefile,tract.mak} tract/doc \
          math/src/{gamma.[ch],chi2.[ch],ruleval.[ch]} \
          math/src/{makefile,math.mak} math/doc \
          util/src/{fntypes.h,error.h} \
          util/src/{arrays.[ch],escape.[ch],symtab.[ch]} \
          util/src/{tabread.[ch],scanner.[ch]} \
          util/src/{makefile,util.mak} util/doc

#-----------------------------------------------------------------------
# Installation
#-----------------------------------------------------------------------
install:
	cp $(PRGS) $(HOME)/bin

#-----------------------------------------------------------------------
# Clean up
#-----------------------------------------------------------------------
localclean:
	rm -f *.o *~ *.flc core $(PRGS)

clean:
	$(MAKE) localclean
	cd $(UTILDIR);  $(MAKE) clean
	cd $(MATHDIR);  $(MAKE) clean
	cd $(TRACTDIR); $(MAKE) clean
