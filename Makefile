CXX := clang++
VERSION ?= 0.1.0
PREFIX ?= /usr/local
DESTDIR ?=

CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Iinclude -DMCLEANER_VERSION=\"$(VERSION)\"
LDFLAGS := -lncurses
SOURCES := src/Tui.cpp src/Cleaner.cpp src/main.cpp
TARGET := mcleaner

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -m 0755 "$(TARGET)" "$(DESTDIR)$(PREFIX)/bin/$(TARGET)"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/$(TARGET)"

.PHONY: all clean install uninstall
