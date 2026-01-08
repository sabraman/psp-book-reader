TARGET = PSP-BookReader
OBJS = src/core/main.o src/core/debug_logger.o lib/pugixml/pugixml.o lib/miniz/miniz.o src/epub/epub_reader.o src/input/input_handler.o src/renderer/text_renderer.o src/parser/html_text_extractor.o lib/libintrafont/intraFont.o lib/libintrafont/libccc.o

INCDIR = include lib/pugixml lib/miniz lib/libintrafont
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpspgu -lpspgum -lpsppower -lpsprtc -lz -lstdc++ -lpng -ljpeg

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP-BookReader
PSP_EBOOT_ICON = NULL
PSP_EBOOT_ADATA = ltn0.pgf

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
