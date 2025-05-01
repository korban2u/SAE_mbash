CC = gcc
CFLAGS = -Wall -Wextra -g -O2
PROGRAM = mbash
SOURCES = src/mbash.c
OBJECTS = $(SOURCES:.c=.o)

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(PROGRAM) $(OBJECTS)

.PHONY: all clean
