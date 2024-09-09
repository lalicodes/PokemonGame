# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c99

# Source files
SRC = maptomap.c

# Executable name
EXE = maptomap

# Libraries
LIBS = -lm

all: $(EXE)

$(EXE): $(SRC)
	$(CC) $(CFLAGS) -o $(EXE) $(SRC) $(LIBS)

clean:
	rm -f $(EXE)

.PHONY: all clean
