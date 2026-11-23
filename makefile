.RECIPEPREFIX = >

CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wundef -Wpointer-arith -Wshadow -Wwrite-strings -Wconversion -I"./src"

ifeq ($(OS),Windows_NT)
    DAEMON_TARGET = yalda_daemon.exe
    CLI_TARGET = yalda.exe
    PROC_SRC = yalda_proc_win32.c
    WIN32_LIBS = -luser32 -lshell32 -lwinhttp -lgdi32
    DAEMON_LDFLAGS = -mwindows
    CLI_LDFLAGS =
    ALL_TARGETS = $(DAEMON_TARGET) $(CLI_TARGET)
    CLEAN_CMD = rm -f *.o src/*.o *.exe
    POST_BUILD = @echo "you're good to go"
else
    DAEMON_TARGET = yalda_daemon
    CLI_TARGET = yalda
    PROC_SRC = yalda_proc_unix.c
    WIN32_LIBS =
    DAEMON_LDFLAGS =
    CLI_LDFLAGS =
    ALL_TARGETS = $(DAEMON_TARGET) $(CLI_TARGET)
    CLEAN_CMD = rm -f *.o src/*.o $(ALL_TARGETS)
    POST_BUILD = @echo "you're good to go"
endif

COMMON_OBJ = src/yalda_tui.o $(PROC_SRC:.c=.o)

.PHONY: all clean

all: $(ALL_TARGETS)
>$(POST_BUILD)

$(DAEMON_TARGET): yalda_daemon.o $(COMMON_OBJ)
>$(CC) $(CFLAGS) $(DAEMON_LDFLAGS) -o $(DAEMON_TARGET) yalda_daemon.o $(COMMON_OBJ) $(WIN32_LIBS)

$(CLI_TARGET): yalda_cli.o $(COMMON_OBJ)
>$(CC) $(CFLAGS) $(CLI_LDFLAGS) -o $(CLI_TARGET) yalda_cli.o $(COMMON_OBJ) $(WIN32_LIBS)

%.o: %.c src/yalda_connection.h
>$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.c src/yalda_connection.h
>$(CC) $(CFLAGS) -c $< -o $@

clean:
>$(CLEAN_CMD)