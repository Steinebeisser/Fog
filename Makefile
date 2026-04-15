CC = gcc # x86_64-w64-mingw32-gcc
CFLAGS = -Wall -Wextra -ggdb -g -O0 -fno-omit-frame-pointer
LDFLAGS = -luring
INCLUDES = -Isrc -I.

PREFIX ?= /usr/local


rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRCS = $(call rwildcard,src,*.c)
OBJS = $(SRCS:src/%.c=build/%.o)

EXE =
WINDOWS_TARGET := false

ifeq ($(OS),Windows_NT)
    WINDOWS_TARGET := true
endif

ifneq (,$(findstring mingw,$(CC)))
    WINDOWS_TARGET := true
endif

ifeq ($(WINDOWS_TARGET),true)
	EXE = .exe
endif


TARGET = fog$(EXE)

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

clean:
	rm -rf build $(TARGET) $(TEST_EXECS)

run: $(TARGET)
	./$(TARGET) $(ARGS)


install: $(TARGET)
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@echo "Installed $(TARGET) to $(DESTDIR)$(PREFIX)/bin/"

uninstall:
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@echo "Uninstalled $(TARGET)"


.PHONY: all clean FORCE
