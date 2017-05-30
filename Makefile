CC=gcc
CFLAGS=-c -Wall -I include -std=c99
SOURCES=$(wildcard src/TotemScript/*.c) src/TotemScriptCmd/main.c
LDFLAGS=-lm
OBJECTS=$(SOURCES:.c=.o)
BIN=totem-cli

all: $(SOURCES) $(BIN)

clean:
	rm $(OBJECTS) $(BIN)

$(BIN): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS) 

.c.o: 
	$(CC) $(CFLAGS) -o $@ $<
