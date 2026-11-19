TARGET = PSP-BookReader
OBJS = src/core/main.o src/core/debug_logger.o lib/pugixml/pugixml.o lib/miniz/miniz.o src/epub/epub_reader.o src/input/input_handler.o src/renderer/text_renderer.o src/renderer/cover_renderer.o src/parser/html_text_extractor.o src/library/library_manager.o

INCDIR = include lib/pugixml lib/miniz $(shell psp-config --psp-prefix)/include/SDL2
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lSDL2_ttf -lharfbuzz -lfreetype -lbz2 -lSDL2_image -lSDL2main -lSDL2 -lGL -lz -lstdc++ -lpng -ljpeg -lpspvfpu -lpsphprm -lpspaudio -lpspvram -lpspgu -lpspgum -lpsppower -lpsprtc -lpspdebug -lpspdisplay -lpspge -lpspctrl

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP-BookReader
PSP_EBOOT_ICON = ICON0.PNG
PSP_EBOOT_ADATA = fonts/Inter-Regular.ttf

PSPSDK=$(shell psp-config --pspsdk-path 2>/dev/null)
# Guarded so host-side tooling (make host-test) works without the PSP
# SDK installed; on a machine with psp-config nothing changes.
ifneq ($(strip $(PSPSDK)),)
include $(PSPSDK)/lib/build.mak
endif

# --- Host-side tests (no PSP toolchain needed) ---
# Regression tests for the HTML extractor and the shared blank-line
# layout logic. The only PSP dependencies (TextStyle via the
# SDL2-needing text_renderer.h; DebugLogger) are stubbed in tests/stubs,
# so this builds and runs on any host.
HOSTCXX ?= c++

.PHONY: host-test
host-test: tests/host_test.cpp tests/stubs/text_renderer.h \
           tests/stubs/debug_logger.h tests/stubs/html_text_extractor.h \
           src/core/blank_line_layout.h \
           src/parser/html_text_extractor.cpp
	cmp -s tests/stubs/html_text_extractor.h include/html_text_extractor.h || { echo 'tests/stubs/html_text_extractor.h is out of sync with include/html_text_extractor.h - refresh it with: cp include/html_text_extractor.h tests/stubs/'; exit 1; }
	$(HOSTCXX) -std=c++11 -Wall -Wextra -gline-tables-only \
	  -fsanitize=address,undefined \
	  -Itests/stubs -Isrc/core -Iinclude tests/host_test.cpp \
	  src/parser/html_text_extractor.cpp -o host-test
	./host-test --fuzz 20000; rc=$$?; rm -f host-test; rm -rf host-test.dSYM; exit $$rc
