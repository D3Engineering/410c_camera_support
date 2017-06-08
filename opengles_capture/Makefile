# Current supported Defines
# CROSS_COMPILE - GCC prefix.
# PREPROCESS - When set generate preprocessor output.
# DEBUG - When set Add debug symbols and remove optimizations.
# CFLAGS - starting GCC flags.

# Required packages/libraries
# libx11-dev
# libxext-dev
# libdrm-dev


CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -std=gnu99 -Iinclude
ifneq ('$(DEBUG)','')
    CFLAGS += -g
else
    CFLAGS += -O2 -W -Wall
endif

OUTDIR := out

LIBS := -l:libGLESv2.so.2 -l:libEGL.so.1 -lX11 -lXext
LDFLAGS := -L/usr/lib/aarch64-linux-gnu

SOURCE := main.c capture.c display.c gles_egl_util.c
SOURCE += $(wildcard uses/*.c)

OBJS = $(patsubst %.c, $(OUTDIR)/%.o, $(SOURCE))


all: capture
.PHONY: all

$(OUTDIR) $(OUTDIR)/uses:
	mkdir -p $@

ifneq ('$(PREPROCESS)','')
PRE_OBJS := $(patsubst %.c, $(OUTDIR)/%.PRE, $(SOURCE)) $(OBJS)
$(OUTDIR)/%.PRE : %.c
	$(CPP) $(CFLAGS) -c -g -o $@ $<
endif

$(OUTDIR)/%.o : %.c
	$(CC) $(CFLAGS) -MD -c -o $@ $<
-include $(OBJS:.o=.d)

capture: $(OUTDIR) $(OUTDIR)/uses $(PRE_OBJS) $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	-rm -r $(OUTDIR)
	-rm capture
