all: MapReduceFramework.a Search

Search.o: Search.cpp
		g++ -Wall -pthread -std=c++11 -g -c Search.cpp -o Search.o

Search: Search.o MapReduceFramework.o
		g++ -Wall -pthread -std=c++11 -g MapReduceFramework.o Search.o -o Search

MapReduceFramework.o: MapReduceFramework.cpp MapReduceFramework.h MapReduceClient.h
	g++ -Wall -pthread -std=c++11 -g -c MapReduceFramework.cpp -o MapReduceFramework.o

MapReduceFramework.a: MapReduceFramework.o
	ar rcs MapReduceFramework.a MapReduceFramework.o

tar:
	tar -cvf ex3.tar Makefile README MapReduceFramework.cpp Search.cpp

clean:
	rm -f *.o *.a

.PHONY: clean all tar
