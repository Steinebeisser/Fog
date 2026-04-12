CC = gcc
CFLAGS = -Wall -Wextra -ggdb -g -O3
INCLUDES = -Isrc -I.


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
	@$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

build/%.o: src/%.c
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

clean:
	rm -rf build $(TARGET) $(TEST_EXECS)

run: $(TARGET)
	./$(TARGET) $(ARGS)



.PHONY: all clean FORCE
