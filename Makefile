TARGET = PSP-BookReader
OBJS = src/core/main.o src/core/debug_logger.o lib/pugixml/pugixml.o lib/miniz/miniz.o src/epub/epub_reader.o src/input/input_handler.o src/renderer/text_renderer.o src/renderer/cover_renderer.o src/parser/html_text_extractor.o src/library/library_manager.o lib/libintrafont/intraFont.o lib/libintrafont/libccc.o

INCDIR = include lib/pugixml lib/miniz lib/libintrafont $(shell psp-config --psp-prefix)/include/SDL2
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lSDL2_ttf -lharfbuzz -lfreetype -lbz2 -lSDL2_image -lSDL2main -lSDL2 -lGL -lz -lstdc++ -lpng -ljpeg -lpspvfpu -lpsphprm -lpspaudio -lpspvram -lpspgu -lpspgum -lpsppower -lpsprtc -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspnet -lpspnet_apctl

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP-BookReader
PSP_EBOOT_ICON = ICON0.PNG
PSP_EBOOT_ADATA = fonts/extras/ttf/Inter-Regular.ttf

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
