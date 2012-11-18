#-----------------------------------------------------------------------
# File    : makefile
# Contents: build item and transaction management (on Unix systems)
# Author  : Christian Borgelt
# History : 2008.10.05 file created from apriori makefile
#           2009.10.09 module pfxtree added
#           2010.02.10 module pfxtree replaced by module clomax
#           2010.10.08 changed standard from -ansi to -std=c99
#           2011.05.06 changed to double support reporting/recording
#           2011.08.24 main program fim16 added (mainly for testing)
#           2012.07.25 module tract with write functions added (trawr)
#-----------------------------------------------------------------------
SHELL   = /bin/bash
THISDIR = ../../tract/src
UTILDIR = ../../util/src

CC      = gcc -std=c99
# CC      = g++
CFBASE  = -Wall -pedantic $(ADDFLAGS)
CFLAGS  = $(CFBASE) -DNDEBUG -O3
# CFLAGS  = $(CFBASE) -DNDEBUG -O3 -DALIGN8
# CFLAGS  = $(CFBASE) -g
# CFLAGS  = $(CFBASE) -g -DALIGN8
INCS    = -I$(UTILDIR)

LD       = gcc
# LD       = g++
LDFLAGS  = 
LIBS     = -lm

HDRS     = $(UTILDIR)/arrays.h  $(UTILDIR)/memsys.h \
           $(UTILDIR)/symtab.h  $(UTILDIR)/escape.h \
           $(UTILDIR)/tabread.h $(UTILDIR)/scanner.h \
           tract.h clomax.h report.h
OBJS     = $(UTILDIR)/arrays.o  $(UTILDIR)/memsys.o \
           $(UTILDIR)/idmap.o   $(UTILDIR)/escape.o \
           $(UTILDIR)/tabread.o $(UTILDIR)/scform.o \
           clomax.o repcm.o
PRGS     = fim16 tract

#-----------------------------------------------------------------------
# Build Program
#-----------------------------------------------------------------------
all:       $(PRGS)

tract:     $(OBJS) tramain.o makefile
	$(LD) $(LDFLAGS) $(OBJS) tramain.o $(LIBS) -o $@

fim16:     $(OBJS) tract.o m16main.o makefile
	$(LD) $(LDFLAGS) $(OBJS) tract.o m16main.o $(LIBS) -o $@

#-----------------------------------------------------------------------
# Main Programs
#-----------------------------------------------------------------------
tramain.o: $(HDRS)
tramain.o: tract.c makefile
	$(CC) $(CFLAGS) $(INCS) -c tract.c -o $@

m16main.o: $(HDRS)
m16main.o: fim16.c fim16.h makefile
	$(CC) $(CFLAGS) $(INCS) -c fim16.c -o $@

#-----------------------------------------------------------------------
# Item and Transaction Management
#-----------------------------------------------------------------------
tract.o:   tract.h $(UTILDIR)/arrays.h $(UTILDIR)/symtab.h
tract.o:   tract.c makefile
	$(CC) $(CFLAGS) $(INCS) -DNOMAIN -c tract.c -o $@

trawr.o:   tract.h $(UTILDIR)/arrays.h $(UTILDIR)/symtab.h
trawr.o:   tract.c makefile
	$(CC) $(CFLAGS) $(INCS) -DNOMAIN -DTA_WRITE -c tract.c -o $@

tatree.o:  tract.h $(UTILDIR)/arrays.h $(UTILDIR)/symtab.h
tatree.o:  tract.c makefile
	$(CC) $(CFLAGS) $(INCS) -DNOMAIN -DTATREEFN -c tract.c -o $@

#-----------------------------------------------------------------------
# Frequent Item Set Mining (with at most 16 items)
#-----------------------------------------------------------------------
fim16.o:   $(HDRS)
fim16.o:   fim16.c makefile
	$(CC) $(CFLAGS) $(INCS) -DNOMAIN -c fim16.c -o $@

#-----------------------------------------------------------------------
# Item Set Reporter Management
#-----------------------------------------------------------------------
report.o:  report.h tract.h $(UTILDIR)/arrays.h $(UTILDIR)/symtab.h
report.o:  report.c makefile
	$(CC) $(CFLAGS) $(INCS) -c report.c -o $@

repdbl.o:  report.h tract.h $(UTILDIR)/arrays.h $(UTILDIR)/symtab.h
repdbl.o:  report.c makefile
	$(CC) $(CFLAGS) $(INCS) -DSUPP_T=double -c report.c -o $@

repcm.o:   report.h tract.h $(UTILDIR)/arrays.h $(UTILDIR)/symtab.h
repcm.o:   report.c makefile
	$(CC) $(CFLAGS) $(INCS) -DISR_CLOMAX -c report.c -o $@

repcmd.o:  report.h tract.h $(UTILDIR)/arrays.h $(UTILDIR)/symtab.h
repcmd.o:  report.c makefile
	$(CC) $(CFLAGS) $(INCS) -DSUPP_T=double -DISR_CLOMAX -c report.c -o $@

#-----------------------------------------------------------------------
# Closed and Maximal Frequent Item Set Tree Management
#-----------------------------------------------------------------------
clomax.o:  clomax.h $(UTILDIR)/arrays.h $(UTILDIR)/memsys.h
clomax.o:  clomax.c makefile
	$(CC) $(CFLAGS) $(INCS) -c clomax.c -o $@

cmdbl.o:   clomax.h $(UTILDIR)/arrays.h $(UTILDIR)/memsys.h
cmdbl.o:   clomax.c makefile
	$(CC) $(CFLAGS) $(INCS) -DSUPP_T=double -c clomax.c -o $@

#-----------------------------------------------------------------------
# External Modules
#-----------------------------------------------------------------------
$(UTILDIR)/arrays.o:
	cd $(UTILDIR);  $(MAKE) arrays.o  ADDFLAGS=$(ADDFLAGS)
$(UTILDIR)/memsys.o:
	cd $(UTILDIR);  $(MAKE) memsys.o  ADDFLAGS=$(ADDFLAGS)
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

#-----------------------------------------------------------------------
# Source Distribution Packages
#-----------------------------------------------------------------------
dist:
	$(MAKE) clean
	cd ../..; rm -f eclat.zip eclat.tar.gz; \
        zip -rq eclat.zip eclat/{src,ex,doc} \
          tract/src/{tract.[ch],report.[ch],clomax.[ch]} \
          tract/src/{makefile,tract.mak} tract/doc \
          util/src/{fntypes.h,error.h} \
          util/src/{arrays.[ch],memsys.[ch],symtab.[ch]} \
          util/src/{escape.[ch],tabread.[ch],scanner.[ch]} \
          util/src/{makefile,util.mak} util/doc; \
        tar cfz eclat.tar.gz eclat/{src,ex,doc} \
          tract/src/{tract.[ch],report.[ch],clomax.[ch]} \
          tract/src/{makefile,tract.mak} tract/doc \
          util/src/{fntypes.h,error.h} \
          util/src/{arrays.[ch],memsys.[ch],symtab.[ch]} \
          util/src/{escape.[ch],tabread.[ch],scanner.[ch]} \
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
