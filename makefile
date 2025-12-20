CC = gcc

DEBUG_MODE = 0;

ifeq ($(DEBUG_MODE), 0)
FLAGS = -std=c11 -Wall -Wextra
else
FLAGS = -g -std=c11 -Wall -Wextra
endif

THREADPOOL = ./threadpool/
CHANNELS = ./channels/

test: main.o threadpool.o spmc.o
	$(CC) ./build/*.o -o ./build/test

main.o: main.c $(THREADPOOL)threadpool.h
	$(CC) $(FLAGS) -c main.c -o ./build/main.o

# Build threadpool 
threadpool.o: $(THREADPOOL)threadpool.h $(THREADPOOL)threadpool.c 
	$(CC) $(FLAGS) -c $(THREADPOOL)threadpool.c -o ./build/threadpool.o

# Build SPSC Channel
spsc.o: $(CHANNELS)spsc.c $(CHANNELS)spsc.h
	$(CC) $(FLAGS) -c $(CHANNELS)spsc.c -o ./build/spsc.o

# Build SPMC Channel
spmc.o: $(CHANNELS)spmc.c $(CHANNELS)spmc.h
	$(CC) $(FLAGS) -c $(CHANNELS)spmc.c -o ./build/spmc.o

# Build MPSC Channel
mpsc.o: $(CHANNELS)mpsc.c $(CHANNELS)mpsc.h
	$(CC) $(FLAGS) -c $(CHANNELS)mpsc.c -o ./build/mpsc.o

# Build MPMC Channel
mpmc.o: $(CHANNELS)mpmc.c $(CHANNELS)mpmc.h
	$(CC) $(FLAGS) -c $(CHANNELS)mpmc.c -o ./build/mpmc.o

clean:
	rm -f ./build/*
	@echo "Cleaned Build File..."

run:
	make
	@echo " "
	./build/test

debug:
	make DEBUG_BUILD=1
	@echo " "

bench_mpsc:
	gcc -O3 -march=native -pthread ./benchmarks/bench_mpsc.c \
        ./channels/mpsc.c \
        -o ./build/bench_mpsc
