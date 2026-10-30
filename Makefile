CC=g++

CFLAGS=-std=c++20

BOOSTL=-lboost_system

all:
	$(CC) $(CFLAGS) main.cpp request.cpp response.cpp connectionManager.cpp Log/log.cpp -o httpServer
oldBoost:
	$(CC) $(CFLAGS)  -o httpServer main.cpp $(BOOSTL)

clean:
	rm httpServer
