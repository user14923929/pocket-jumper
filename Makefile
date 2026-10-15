ifeq ($(BUILD_INNER),1)

TARGET = EBOOT
OBJS = main.o
VPATH = ../src

CFLAGS = -O2 -G0 -Wall -Wextra
LIBS = -lpspgu -lpspaudio -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Pocket Jumper

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

else

.PHONY: all clean

all:
	mkdir -p build
	$(MAKE) -C build -f ../Makefile BUILD_INNER=1

clean:
	rm -rf build

endif
