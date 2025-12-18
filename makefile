CC = gcc
FLAGS = -std=c11 -Wall -Wextra

test: main.o threadpool.o
	$(CC) ./build/*.o -o ./build/test

main.o: main.c ./threadpool/threadpool.h
	$(CC) $(FLAGS) -c main.c -o ./build/main.o

# Build threadpool 
threadpool.o: ./threadpool/threadpool.h ./threadpool/threadpool.c 
	$(CC) $(FLAGS) -c ./threadpool/threadpool.c -o ./build/threadpool.o

# Build SPSC Channel
spsc.o: spsc.c spsc.h
	$(CC) $(FLAGS) -c ./channels/spsc.c -o ./build/spsc.o

# Build SPMC Channel
spmc.o: spmc.c spmc.h
	$(CC) $(FLAGS) -c ./channels/spmc.c -o ./build/spmc.o

# Build MPSC Channel
mpsc.o: mpsc.c mpsc.h
	$(CC) $(FLAGS) -c ./channels/mpsc.c -o ./build/mpsc.o

# Build MPMC Channel
mpmc.o: mpmc.c mpmc.h
	$(CC) $(FLAGS) -c ./channels/mpmc.c -o ./build/mpmc.o

run:
	make
	@echo " "
	./build/test
