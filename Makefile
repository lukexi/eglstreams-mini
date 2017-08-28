INCLUDE_DIRS = -I/usr/include/libdrm -Isrc
LIBS         = -lEGL -lOpenGL -ldrm -lGLEW -lm -lturbojpeg -lpthread

SHADERS += $(wildcard shaders/*)
SOURCES += $(wildcard src/*.c)
OBJECTS += $(SOURCES:src/%.c=build/%.o)
DEPENDS := $(SOURCES:src/%.c=build/%.d)

CFLAGS += -Wall
CFLAGS += -Wno-unused-function
CFLAGS += -Wno-missing-braces
CFLAGS += -O3
# Debugging support
CFLAGS += -g
# Needed for creating .so files
CFLAGS += -fPIC
# Needed for Linux's "perf" to work correctly
# CFLAGS += -fno-omit-frame-pointer

MAINS = $(wildcard ./*.c)
APPS = $(MAINS:./%.c=./%.app)

all: build/ $(APPS)

-include $(DEPENDS)

build/:
	mkdir -p build

# Build each c file into a .o in the build dir
# (this caches them for faster rebuilds)
build/%.o: src/%.c
	clang -MMD -c -o $@ $< $(CFLAGS) $(INCLUDE_DIRS)

%.app: ./%.c $(OBJECTS)
	clang -o $@ $^ $(CFLAGS) $(INCLUDE_DIRS) $(LIBS)

clean:
	rm -rf build/
	rm -f *.app
