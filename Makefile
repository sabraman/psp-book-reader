TARGET = PSP-BookReader
OBJS = src/core/main.o lib/pugixml/pugixml.o lib/miniz/miniz.o src/epub/epub_reader.o src/input/input_handler.o

INCDIR = include lib/pugixml lib/miniz
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpspgu -lpspgum -lz -lstdc++

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP-BookReader
PSP_EBOOT_ICON = NULL

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
