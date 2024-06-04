CC=g++

CFLAGS=-std=c++20

BOOSTL=-lboost_system

all:
	$(CC) $(CFLAGS) -o httpServer main.cpp $(BOOSTL)

clean:
	rm httpServer
