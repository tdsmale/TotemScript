CC=gcc
CFLAGS=-MMD -c -Wall -I include -std=c99
LDFLAGS=-lm

SOURCES=$(wildcard src/TotemScript/*.c)
OBJECTS=$(SOURCES:.c=.o)
DEPS=$(SOURCES:.c=.d)
CMD_SOURCES=src/TotemScriptCmd/main.c
CMD_OBJECTS=$(CMD_SOURCES:.c=.o)
CMD_DEPS=$(CMD_SOURCES:.c=.d)
TEST_SOURCES=src/TotemScriptTest/test.c
TEST_OBJECTS=$(TEST_SOURCES:.c=.o)
TEST_DEPS=$(TEST_SOURCES:.c=.d)

BIN=TotemScriptCmd
TEST_BIN=src/TotemScriptTest/TotemScriptTest

all: $(SOURCES) $(BIN) test

clean:
	rm -f $(OBJECTS) $(DEPS) $(BIN)
	rm -f $(TEST_OBJECTS) $(TEST_DEPS) $(TEST_BIN)

$(BIN): $(OBJECTS) $(CMD_OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(CMD_OBJECTS) $(LDFLAGS)

-include $(DEPS)
-include $(CMD_DEPS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ $<

$(TEST_BIN): $(OBJECTS) $(TEST_OBJECTS) $(BIN)
	$(CC) -o $@ $(OBJECTS) $(TEST_OBJECTS) $(LDFLAGS)
	cp TotemScriptCmd src/TotemScriptTest

-include $(TEST_DEPS)

test: $(TEST_BIN)
