CC = gcc
CXX = g++
ECHO = echo
RM = rm -f

CFLAGS = -Wall -ggdb -funroll-loops 
CXXFLAGS = -Wall -ggdb -funroll-loops 

LDFLAGS = -lncurses

BIN = poke327
OBJS = poke327.o heap.o character.o io.o parse.o

all: $(BIN) 

$(BIN): $(OBJS)
	@$(ECHO) Linking $@
	@$(CXX) $^ -o $@ $(LDFLAGS)

-include $(OBJS:.o=.d)

%.o: %.c
	@$(ECHO) Compiling $<
	@$(CC) $(CFLAGS) -MMD -MF $*.d -c $<

%.o: %.cpp
	@$(ECHO) Compiling $<
	@$(CXX) $(CXXFLAGS) -MMD -MF $*.d -c $<

.PHONY: all clean clobber 

clean:
	@$(ECHO) Removing all generated files
	@$(RM) *.o $(BIN) *.d TAGS core vgcore.* gmon.out

clobber: clean
	@$(ECHO) Removing backup files
	@$(RM) *~ \#* *pgm

