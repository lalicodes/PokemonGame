CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = -lm
TARGET = imageprocessing

SRCS = imageprocessing.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $<

motorcycle: $(TARGET)
	./$(TARGET) motorcycle.pgm motorcycle.edge.pgm

bigger_digger: $(TARGET)
	./$(TARGET) bigger_digger.pgm bigger_digger.edge.pgm

clean:
	rm -f $(OBJS) $(TARGET)
