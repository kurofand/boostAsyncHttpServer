CC=g++

CFLAGS=-std=c++20

BOOSTL=-lboost_system

PARSERS=Parsers/multipartParser.cpp Parsers/urlencodedParser.cpp Parsers/plainParser.cpp Parsers/jsonParser.cpp

all:
	$(CC) $(CFLAGS) main.cpp request.cpp response.cpp connectionManager.cpp utils.cpp $(PARSERS) Log/log.cpp -o httpServer
oldBoost:
	$(CC) $(CFLAGS)  -o httpServer main.cpp $(BOOSTL)

clean:
	rm httpServer
