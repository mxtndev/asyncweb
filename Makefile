CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c17 -O2
LDFLAGS =
SOURCES = main.c server.c http.c utils.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = asyncweb

.PHONY: all clean

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)