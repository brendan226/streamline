CC=gcc -O3

CFLAGS= -Iinclude
LDFLAGS=

SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=bin

.PHONY: all shared clean

all: streamline

run: all
	$(BIN)/streamline.exe

streamline: $(OBJ)
	$(CC) -o $(BIN)/streamline.exe $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

static: $(OBJ)
	ar rcs $(BIN)/libstreamline.a $^

clean:
	del /f bin\streamline.exe bin\*.a src\*.o
