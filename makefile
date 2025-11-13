# Compiler and flags
CC = gcc
CFLAGS = -std=c90 -Wall -ansi -pedantic -Wunused

# Sources / objects / target 
SRCS  = main.c first_pass.c functions.c process_macros.c prints.c second_pass.c opmodes.c words.c diag.c cleanup.c
OBJS  = $(SRCS:.c=.o)
EXEC  = assembler

# --- OS-specific settings ---
ifeq ($(OS),Windows_NT)
  RM      = del /Q
  EXEEXT  = .exe
  NULLDEV = NUL
  SEP     = &
else
  RM    = rm -f
  EXEEXT  =
  NULLDEV = /dev/null
  SEP     = ;
endif

# Default: just build
all: $(EXEC)$(EXEEXT)

# Link
$(EXEC)$(EXEEXT): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Compile
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean (cross-platform)
clean:
	rm -f $(OBJS) $(EXEC) *.am *.ob *.ent *.ext

# Optional: clean + build + run (usage: mingw32-make rebuild-run FILE=course_input)
FILE ?= course_input
rebuild-run: clean $(EXEC)$(EXEEXT)
	./$(EXEC)$(EXEEXT) $(FILE)
