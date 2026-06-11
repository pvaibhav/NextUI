###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=tg5040 make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

###########################################################

include ../platform/makefile.env

###########################################################

TARGET = displaycal
INCDIR = -I. -I../../all/common/ -I../platform/
SOURCE = $(TARGET).c ../../all/common/utils.c ../../all/common/api.c ../../all/common/config.c ../../all/common/scaler.c ../platform/platform.c

CC = $(CROSS_COMPILE)gcc
CFLAGS += -mcpu=cortex-a53 -flto $(OPT) $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99
LDFLAGS += -lmsettings -lm -s

PRODUCT = build/$(PLATFORM)/$(TARGET).elf

all: $(PREFIX_LOCAL)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PRODUCT)

$(PREFIX_LOCAL)/include/msettings.h:
	cd ../libmsettings && make
