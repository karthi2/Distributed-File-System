all : server master client

server : server.c mysocket.o
	gcc -g -pthread -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -o dfsserver server.c mysocket.o

master : master.c mysocket.o master.h
	gcc -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -o dfsmaster master.c mysocket.o

client : client.c mysocket.o
	gcc -g -w `pkg-config fuse --cflags --libs` -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -o dfsclient client.c mysocket.o

mysocket.o : mysocket.c mysocket.h
	gcc -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -c mysocket.c

#log.o : log.c log.h params.h
#	gcc -g -m32 -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE `pkg-config fuse --cflags` -c log.c

clean:
	rm -f dfsserver dfsmaster dfsclient *.o

dist:
	rm -rf fuse-tutorial/
	mkdir fuse-tutorial/
	cp ../*.html fuse-tutorial/
	mkdir fuse-tutorial/example/
	mkdir fuse-tutorial/example/mountdir/
	mkdir fuse-tutorial/example/rootdir/
	echo "a bogus file" > fuse-tutorial/example/rootdir/bogus.txt
	mkdir fuse-tutorial/src
	cp Makefile *.c *.h fuse-tutorial/src/
	tar cvzf ../../fuse-tutorial.tgz fuse-tutorial/
	rm -rf fuse-tutorial/
