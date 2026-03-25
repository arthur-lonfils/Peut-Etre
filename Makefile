CC       = gcc
CFLAGS   = -Wall -Wextra -pedantic -std=c11 -g -O1
SRCDIR   = src
BUILDDIR = build
SOURCES  = $(wildcard $(SRCDIR)/*.c)
OBJECTS  = $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
TARGET   = peut-etre

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

test: $(TARGET)
	./tests/run_tests.sh

clean:
	rm -rf $(BUILDDIR) $(TARGET)
