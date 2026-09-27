CC=g++

CFLAGS=-std=c++20

BOOSTL=-lboost_system

all:
	$(CC) $(CFLAGS) main.cpp request.cpp response.cpp -o httpServer
oldBoost:
	$(CC) $(CFLAGS)  -o httpServer main.cpp $(BOOSTL)

clean:
	rm httpServer
