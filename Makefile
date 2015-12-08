CC = clang
CFLAGS = -Werror -Wall -Wextra -Wconversion -std=c11 -O3 -D _MIXER_LINEAR
BINARY = modterm
LIBS = ../portaudio/lib/.libs/libportaudio.a -lm -pthread -lasound
IMPORT = -I ../portaudio/include/

SRC_FILES = $(wildcard *.c)
OBJ_FILES = $(SRC_FILES:.c=.o)


all: $(BINARY)
	

.PHONY: clean
clean:
	rm -f $(OBJ_FILES)

$(BINARY): $(OBJ_FILES)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(IMPORT)
