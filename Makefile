# Makefile for NotesApp

OUT = notes_app.app
SRC = notes_app.cpp app.cpp ui.cpp storage.cpp export.cpp utils.cpp
STD = -std=gnu++14
LDFLAGS = -Wl,--no-as-needed -Wl,--copy-dt-needed-entries -lm -Wl,--as-needed -linkview -lsqlite3 -lpthread

SDKPATH_FILE := sdkpath.txt
SDKDIR := $(shell if [ -f $(SDKPATH_FILE) ]; then cat $(SDKPATH_FILE); fi)
ifeq ($(strip $(SDKDIR)),)
$(error $(SDKPATH_FILE) not found or empty — create it with the SDK base path)
endif

all: $(OUT)

$(OUT): $(SRC)
	$(SDKDIR)/usr/bin/arm-obreey-linux-gnueabi-g++ $(SRC) -o $(OUT) $(STD) $(LDFLAGS)

clean:
	rm -f $(OUT)

run: $(OUT)
	./$(OUT)

.PHONY: all clean run
