TARGET = cube
OBJS = cube.o logo.o common/callbacks.o polyphonic.o msgdialog.o

INCDIR =
CFLAGS = -Wall -O0
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS= -lpspgum -lpspgu -lpspaudiolib -lpspaudio

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = LANDMINE

# let CFW decide this
PSP_LARGE_MEMORY = 0

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak


logo.o: logo.raw
	bin2o -i logo.raw logo.o logo
