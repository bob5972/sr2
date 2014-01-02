# -*- mode: Makefile;-*-
# Makefile --
# "Look on my works, ye Mighty, and despair!" --Ozymandias

include config.mk
-include local.mk

SRCROOT=.
INCLUDE_FLAGS=-I MBLib/public

SRCDIR=$(SRCROOT)
BUILDDIR=$(BUILDROOT)
DEPDIR=$(DEPROOT)

#Final binary
TARGET=result

#The BUILDROOT folder is included for config.h
CFLAGS = ${DEFAULT_CFLAGS} -I $(BUILDROOT) $(INCLUDE_FLAGS)
CPPFLAGS = ${CFLAGS}

LIBFLAGS =

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	${CC} -c ${CFLAGS} -o $(BUILDDIR)/$*.o $<;
$(BUILDDIR)/%.opp: $(SRCDIR)/%.cpp
	${CXX} -c ${CPPFLAGS} -o $(BUILDDIR)/$*.opp $<;

#Autogenerate dependencies information
#The generated makefiles get sourced into this file later
ifeq ($(CC), gcc)
$(DEPDIR)/%.d: $(SRCDIR)/%.c Makefile
	$(CC) -M -MM -MT "$(BUILDDIR)/$*.o" -MT "$(DEPDIR)/$*.d" -MF $@ ${CFLAGS} $<;
$(DEPROOT)/%.dpp: $(SRCDIR)/%.cpp Makefile
	$(CXX) -M -MM -MT "$(BUILDDIR)/$*.opp" \
	    -MT "$(DEPDIR)/$*.dpp" \
	    -MF $@ ${CPPFLAGS} $<;
endif

#No paths. VPATH is assumed
C_SOURCES = main.c
CPP_SOURCES = 

#For reasons I cannot fathom, MBLIB_OBJ has to be last
# or things don't link right...

MBLIB_OBJ = $(MBLIB_BUILDDIR)/MBLib.a

C_OBJ=$(addprefix $(BUILDDIR)/, $(subst .c,.o, $(C_SOURCES)))
CPP_OBJ=$(addprefix $(BUILDDIR)/, $(subst .cpp,.opp, $(CPP_SOURCES)))
TARGET_OBJ = $(C_OBJ) $(CPP_OBJ) $(MBLIB_OBJ)
		     
.PHONY: all clean distclean dist MBLib

#The config check is to test if we've been configured
all: config.mk $(BUILDROOT)/config.h $(TARGET)

$(MBLIB_OBJ): MBLib

MBLib:
	$(MAKE) -f $(MBLIB_SRCDIR)/Makefile all

$(TARGET): $(BUILDROOT)/$(TARGET)

$(BUILDROOT)/$(TARGET): $(TARGET_OBJ)
	${CXX} ${CFLAGS} ${LIBFLAGS} $(TARGET_OBJ) -o $(BUILDROOT)/$(TARGET)

# XXX: I don't yet have a way to auto create the build dirs before
#      building... 
# clean leaves the dep files
clean:
	$(MAKE) -f $(MBLIB_SRCDIR)/Makefile clean
	rm -f $(TMPDIR)/*
	rm -f $(BUILDDIR)/*.o $(BUILDDIR)/*.opp
	rm -f $(BUILDROOT)/$(TARGET)

# after a distclean you'll need to run configure again
distclean: clean
	rm -rf $(BUILDROOT)/
	rm -rf config.mk
	
install: $(TARGET)
	./install.sh

#include the generated dependency files
ifeq ($(CC), gcc)
-include $(addprefix $(DEPDIR)/,$(subst .c,.d,$(C_SOURCES)))
-include $(addprefix $(DEPDIR)/,$(subst .cpp,.dpp,$(CPP_SOURCES)))
endif

