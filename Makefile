TARGET = cube
OBJS = cube.o logo.o common/callbacks.o polyphonic.o

INCDIR =
CFLAGS = -Wall -O0
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS= -lpspgum -lpspgu -lpspaudiolib -lpspaudio

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = LANDMINE

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

logo.o: logo.raw
	bin2o -i logo.raw logo.o logo
