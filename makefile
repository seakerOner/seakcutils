CC = gcc

DEBUG_MODE = 0;

ifeq ($(DEBUG_MODE), 0)
FLAGS = -std=c11 -Wall -Wextra
else
FLAGS = -g -std=c11 -Wall -Wextra
endif

BUILD = ./build/
THREADPOOL = ./threadpool/
CHANNELS = ./channels/
ARENAS = ./arenas/

test: main.o threadpool.o spmc.o
	$(CC) $(BUILD)*.o -o $(BUILD)/test

main.o: main.c $(THREADPOOL)threadpool.h
	$(CC) $(FLAGS) -c main.c -o $(BUILD)main.o

# Build threadpool 
threadpool.o: $(THREADPOOL)threadpool.h $(THREADPOOL)threadpool.c 
	$(CC) $(FLAGS) -c $(THREADPOOL)threadpool.c -o $(BUILD)threadpool.o

# Build SPSC Channel
spsc.o: $(CHANNELS)spsc.c $(CHANNELS)spsc.h
	$(CC) $(FLAGS) -c $(CHANNELS)spsc.c -o $(BUILD)spsc.o

# Build SPMC Channel
spmc.o: $(CHANNELS)spmc.c $(CHANNELS)spmc.h
	$(CC) $(FLAGS) -c $(CHANNELS)spmc.c -o $(BUILD)spmc.o

# Build MPSC Channel
mpsc.o: $(CHANNELS)mpsc.c $(CHANNELS)mpsc.h
	$(CC) $(FLAGS) -c $(CHANNELS)mpsc.c -o $(BUILD)mpsc.o

# Build MPMC Channel
mpmc.o: $(CHANNELS)mpmc.c $(CHANNELS)mpmc.h
	$(CC) $(FLAGS) -c $(CHANNELS)mpmc.c -o $(BUILD)mpmc.o

# Build Generic Arena
arena.o: $(ARENAS)arena.c $(ARENAS)arena.h
	$(CC) $(FLAGS) -c $(ARENAS)arena.c -o $(BUILD)arena.o

# Build String Arena
s_arena.o: $(ARENAS)string_arena.c $(ARENAS)string_arena.h
	$(CC) $(FLAGS) -c $(ARENAS)string_arena.c -o $(BUILD)string_arena.o

clean:
	rm -f $(BUILD)*
	@echo "Cleaned Build Files..."

run:
	make
	@echo " "
	./build/test

debug:
	make DEBUG_BUILD=1
	@echo " "

bench_mpsc:
	gcc -O3 -march=native -pthread ./benchmarks/bench_mpsc.c \
        $(CHANNELS)mpsc.c \
        -o $(BUILD)bench_mpsc
