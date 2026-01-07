CC = gcc

DEBUG_MODE = 0;

ifeq ($(DEBUG_MODE), 0)
FLAGS = -std=c11 -Wall -Wextra
else
FLAGS = -g -std=c11 -Wall -Wextra
endif

BUILD = ./build/
THREADPOOL = ./threadpool/
JOBSYSTEM = ./job_system/
WG = ./wait_group/
YIELD = ./yield/
CHANNELS = ./channels/
ARENAS = ./arenas/
DATA_STRUCTURES = ./data_structures/

test: main.o
	$(CC) $(BUILD)*.o -o $(BUILD)/test

main.o: main.c
	$(CC) $(FLAGS) -c main.c -o $(BUILD)main.o

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

bench_job_system:
	gcc -O3 -march=native -pthread ./benchmarks/job_system/job_sys_parallel_bench.c \
        $(JOBSYSTEM)jobsystem.c \
	$(CHANNELS)mpmc.c \
	$(ARENAS)r_arena.c \
	$(THREADPOOL)threadpool.c \
        -o $(BUILD)bench_job_system
