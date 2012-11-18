#-----------------------------------------------------------------------
# File    : apriori.mak
# Contents: build apriori program (on Windows systems)
# Author  : Christian Borgelt
# History : 2003.01.26 file created
#           2006.07.20 adapted to Visual Studio 8
#           2010.08.22 module escape added (for module tabread)
#           2011.07.22 module ruleval added (rule evaluation)
#           2011.10.18 special program version apriacc added
#-----------------------------------------------------------------------
THISDIR  = ..\..\apriori\src
UTILDIR  = ..\..\util\src
MATHDIR  = ..\..\math\src
TRACTDIR = ..\..\tract\src

CC       = cl.exe
DEFS     = /D WIN32 /D NDEBUG /D _CONSOLE /D _CRT_SECURE_NO_WARNINGS
CFLAGS   = /nologo /W3 /O2 /GS- $(DEFS) /c
INCS     = /I $(UTILDIR) /I $(MATHDIR) /I $(TRACTDIR)

LD       = link.exe
LDFLAGS  = /nologo /subsystem:console /incremental:no
LIBS     = 

HDRS     = $(UTILDIR)\arrays.h    $(UTILDIR)\symtab.h  \
           $(UTILDIR)\escape.h    $(UTILDIR)\tabread.h \
           $(UTILDIR)\scanner.h   $(MATHDIR)\gamma.h   \
           $(MATHDIR)\chi2.h      $(MATHDIR)\ruleval.h \
           $(TRACTDIR)\tract.h    $(TRACTDIR)\report.h \
           istree.h apriori.h
OBJS     = $(UTILDIR)\arrays.obj  $(UTILDIR)\idmap.obj   \
           $(UTILDIR)\escape.obj  $(UTILDIR)\tabread.obj \
           $(UTILDIR)\scform.obj  $(MATHDIR)\gamma.obj   \
           $(MATHDIR)\chi2.obj    $(MATHDIR)\ruleval.obj \
           $(TRACTDIR)\tatree.obj $(TRACTDIR)\report.obj \
           istree.obj
PRGS     = apriori.exe apriacc.exe

#-----------------------------------------------------------------------
# Build Programs
#-----------------------------------------------------------------------
all:         $(PRGS)

apriori.exe: $(OBJS) apriori.obj
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) apriori.obj /out:$@

apriacc.exe: $(OBJS) apriacc.obj
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) apriacc.obj /out:$@

#-----------------------------------------------------------------------
# Main Programs
#-----------------------------------------------------------------------
apriori.obj: $(HDRS) apriori.c apriori.mak
	$(CC) $(CFLAGS) $(INCS) apriori.c /Fo$@

apriacc.obj: $(HDRS) apriori.c apriori.mak
	$(CC) $(CFLAGS) $(INCS) /D APRIACC apriori.c /Fo$@

#-----------------------------------------------------------------------
# Frequent Item Set Tree Management
#-----------------------------------------------------------------------
istree.obj:  $(MATHDIR)\gamma.h $(TRACTDIR)\tract.h \
             istree.h istree.c apriori.mak
	$(CC) $(CFLAGS) $(INCS) istree.c /Fo$@

#-----------------------------------------------------------------------
# External Modules
#-----------------------------------------------------------------------
$(UTILDIR)\arrays.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak arrays.obj
	cd $(THISDIR)
$(UTILDIR)\idmap.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak idmap.obj
	cd $(THISDIR)
$(UTILDIR)\escape.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak escape.obj
	cd $(THISDIR)
$(UTILDIR)\tabread.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak tabread.obj
	cd $(THISDIR)
$(UTILDIR)\scform.obj:
	cd $(UTILDIR)
	$(MAKE) /f util.mak scform.obj
	cd $(THISDIR)
$(MATHDIR)\gamma.obj:
	cd $(MATHDIR)
	$(MAKE) /f math.mak gamma.obj
	cd $(THISDIR)
$(MATHDIR)\chi2.obj:
	cd $(MATHDIR)
	$(MAKE) /f math.mak chi2.obj
	cd $(THISDIR)
$(MATHDIR)\ruleval.obj:
	cd $(MATHDIR)
	$(MAKE) /f math.mak ruleval.obj
	cd $(THISDIR)
$(TRACTDIR)\tatree.obj:
	cd $(TRACTDIR)
	$(MAKE) /f tract.mak tatree.obj
	cd $(THISDIR)
$(TRACTDIR)\report.obj:
	cd $(TRACTDIR)
	$(MAKE) /f tract.mak report.obj
	cd $(THISDIR)

#-----------------------------------------------------------------------
# Install
#-----------------------------------------------------------------------
install:
	-@copy apri*.exe ..\..\..\bin

#-----------------------------------------------------------------------
# Clean up
#-----------------------------------------------------------------------
localclean:
	-@erase /Q *~ *.obj *.idb *.pch $(PRGS)

clean:
	$(MAKE) /f apriori.mak localclean
	cd $(TRACTDIR)
	$(MAKE) /f tract.mak clean
	cd $(UTILDIR)
	$(MAKE) /f util.mak clean
	cd $(MATHDIR)
	$(MAKE) /f math.mak clean
	cd $(THISDIR)
