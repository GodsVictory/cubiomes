CC      = gcc
AR      = ar
ARFLAGS = cr
override LDFLAGS = -lm
override CFLAGS += -Wall -fwrapv -march=x86-64 -02
#override CFLAGS += -DUSE_SIMD

ifeq ($(OS),Windows_NT)
	override CFLAGS += -D_WIN32
	RM = del
else
	override LDFLAGS += -pthread
	RM = rm
endif

.PHONY : all debug libcubiomes clean

all: god searcher server libcubiomes find_compactbiomes find_quadhuts gen_image.wasm

debug: CFLAGS += -DDEBUG -O0 -ggdb3
debug: god

libcubiomes: CFLAGS += -fPIC
libcubiomes: layers.o generator.o finders.o util.o
	$(AR) $(ARFLAGS) libcubiomes.a $^

find_compactbiomes: find_compactbiomes.o layers.o generator.o finders.o
	$(CC) -o $@ $^ $(LDFLAGS)

find_compactbiomes.o: find_compactbiomes.c
	$(CC) -c $(CFLAGS) $<

find_quadhuts: find_quadhuts.o layers.o generator.o finders.o
	$(CC) -o $@ $^ $(LDFLAGS)

find_quadhuts.o: find_quadhuts.c
	$(CC) -c $(CFLAGS) $<

god: Gods_seedfinder.o layers.o generator.o finders.o util.o
	$(CC) -o $@ $^ $(LDFLAGS)

Gods_seedfinder.o: Gods_seedfinder.c
	$(CC) -c $(CFLAGS) $<

searcher: searcher.o layers.o generator.o finders.o
	$(CC) -o $@ $^ $(LDFLAGS)

searcher.o: searcher.c
	$(CC) -c $(CFLAGS) $<

server: server.o layers.o generator.o finders.o
	$(CC) -o $@ $^ $(LDFLAGS)

server.o: server.c
	$(CC) -c $(CFLAGS) $<

gen_image.wasm: finders.c layers.c generator.c util.c gen_image.c
	emcc -o $@ -Os $^ -s WASM=1

xmapview.o: xmapview.c xmapview.h
	$(CC) -c $(CFLAGS) $<

finders.o: finders.c finders.h
	$(CC) -c $(CFLAGS) $<

generator.o: generator.c generator.h
	$(CC) -c $(CFLAGS) $<

layers.o: layers.c layers.h
	$(CC) -c $(CFLAGS) $<

util.o: util.c util.h
	$(CC) -c $(CFLAGS) $<

clean:
	$(RM) *.o searcher server god find_quadhuts find_compactbiomes libcubiomes gen_image.wasm

