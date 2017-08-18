INCLUDE_DIRS = -I/usr/include/libdrm -Isrc
LIBS         = -lEGL -lOpenGL -ldrm -lGLEW -lm

SHADERS += $(wildcard shaders/*)
HEADERS += $(wildcard src/*.h)
SOURCES += src/egl-device.c
SOURCES += src/egl-display.c
SOURCES += src/egl-modeset.c
SOURCES += src/utils.c
OBJECTS += $(SOURCES:src/%.c=build/%.o)

CFLAGS += -Wall
# CFLAGS += -O3
# Debugging support
CFLAGS += -g
# Needed for creating .so files
CFLAGS += -fPIC

all: build/ egltest

build/:
	mkdir -p build

# Build each c file into a .o in the build dir
# (this caches them for faster rebuilds)
build/%.o: src/%.c $(HEADERS)
	clang -c -o $@ $< $(CFLAGS) $(INCLUDE_DIRS)

egltest: main.c $(OBJECTS)
	clang -o $@ $^ $(CFLAGS) $(INCLUDE_DIRS) $(LIBS)

clean:
	rm -rf build/
	rm -f egltest
