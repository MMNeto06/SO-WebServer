#source: https://makefiletutorial.com/#commands-and-execution + trab1aed

# make              # to compile files and create the executables
# make clean        # to cleanup object files and executables
# make cleanobj     # to cleanup object files only

CFLAGS = -Wall -Wextra -pthread#-O2 -g

PROGS = master

# Default rule: make all programs
all: $(PROGS)

master: config.o semaphores.o shared_mem.o thread_pool.o worker.o http.o stats.o cache.o logger.o master.o 

main.o: config.h semaphores.h shared_mem.h thread_pool.h worker.h http.h stats.h cache.h logger.h master.c 

# Rule to make any .o file dependent upon corresponding .h file
%.o: %.h

# Make uses builtin rule to create .o from .c files.

cleanobj:
	rm -f *.o

clean: cleanobj
	rm -f $(PROGS)

#a ser retirado no futuro
#all: main config.o master.o semaphores.o shared_mem.o
#
#main: main.c config.o master.o
#	cc -Wall main.c -o main
#
#config.o: config.c
#	cc -Wall -c config.c -o config.o
#
#master.o: master.c semaphores.o shared_mem.o
#	cc -Wall -c master.c -o master.o
#
#semaphores.o: sempahores.c
#	cc -Wall -c semaphores.c -o semaphores.o
#
#shared_mem.o: shared_mem.c
#	cc -Wall -c shared_mem.c -o shared_mem.o