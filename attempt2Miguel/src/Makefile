#source: https://makefiletutorial.com/#commands-and-execution + trab1aed

CFLAGS = -Wall -Wextra -pthread

PROGS = master

# Default rule
all: $(PROGS)

master: config.o semaphores.o shared_mem.o thread_pool.o worker.o http.o stats.o cache.o logger.o sse.o master.o

master.o: config.h semaphores.h shared_mem.h thread_pool.h worker.h http.h stats.h cache.h logger.h sse.h master.c

# Rule to make any .o file dependent upon corresponding .h file
%.o: %.h

# --- Build modes ---

debug: CFLAGS += -g
debug: clean all

release: CFLAGS += -O3
release: clean all

# --- Running ---

run: all
	./master

# --- Testing ---

test: run
	../tests/test_suite.sh

# --- Valgrind ---

valgrind: debug
	valgrind --leak-check=full ./master

helgrind: debug
	valgrind --tool=helgrind ./master

# --- Cleaning ---

cleanobj:
	rm -f *.o

clean: cleanobj
	rm -f $(PROGS)
