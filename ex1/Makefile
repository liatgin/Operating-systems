
all: libosm.a

clean:
	rm -f *.o *.a
	
osm.o:
	gcc -Wall -c osm.c -o osm.o

libosm.a: osm.o
	ar rcs libosm.a osm.o

tar:
	tar -cvf ex1.tar Makefile README osm.c
	


.PHONY: clean all tar
