CC = gcc
CFLAGS = -Wall
LDFLAGS = -lncurses
TARGET = userinter
SOURCE = userinter.c heap.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
