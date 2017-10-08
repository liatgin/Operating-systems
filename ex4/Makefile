all: CachingFileSystem

CachingFileSystem: CachingFileSystem.cpp
	g++ -Wall CachingFileSystem.cpp `pkg-config fuse --cflags --libs` -o CachingFileSystem

tar:
	tar -cvf ex4.tar Makefile README CachingFileSystem.cpp

clean:
	rm -f *.o *.a CachingFileSystem

.PHONY: clean all tar

