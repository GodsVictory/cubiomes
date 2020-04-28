CC      = emcc
AR      = ar
ARFLAGS = cr
override LDFLAGS = -lm -O3 -s ALLOW_MEMORY_GROWTH=1 -s EXIT_RUNTIME=1
override CFLAGS += -Wall -fwrapv -march=native -O3 -s ALLOW_MEMORY_GROWTH=1 -s EXIT_RUNTIME=1
#override CFLAGS += -DUSE_SIMD

ifeq ($(OS),Windows_NT)
	override CFLAGS += -D_WIN32
	RM = del
else
	#override LDFLAGS += -pthread
	#RM = rm
endif

.PHONY : all debug libcubiomes clean

all: CFLAGS += -O3 -march=native
all: find_quadhuts find_compactbiomes clean

debug: CFLAGS += -DDEBUG -O0 -ggdb3
debug: find_quadhuts find_compactbiomes clean

libcubiomes: CFLAGS += -O3 -fPIC
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

seedfinder.html: Gods_seedfinder.o layers.o generator.o finders.o
	$(CC) -o $@ $^ $(LDFLAGS)

god: Gods_seedfinder.o layers.o generator.o finders.o
	$(CC) -o $@ $^ $(LDFLAGS)

Gods_seedfinder.o: Gods_seedfinder.c
	$(CC) -c $(CFLAGS) -D WASM=1 $<
	
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
	$(RM) *.o

