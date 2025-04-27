CC=gcc
CFLAGS=-O2 -Wall -std=c99
INCLUDES=-I/usr/include/freetype2 -I/usr/X11R6/include
LINKS=-lfontconfig -lXft -L/usr/X11R6/lib -lX11
LDFLAGS=${INCLUDES} ${LINKS}

SOURCES = \
					src/main.c \
					src/client.c

OBJECTS = $(SOURCES:.c=.o)

OUTPUT_PATH=build/wm

all: $(OUTPUT_PATH)

$(OUTPUT_PATH): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(OUTPUT_PATH) $(LDFLAGS)

clean:
	rm $(OUTPUT_PATH) $(OBJECTS)
