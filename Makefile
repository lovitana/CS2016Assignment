CFLAGS+=-Wall -pedantic -g
CC= gcc
all: remove main

main: main.o kernel.o llist.o prioll.o

remove: 
	rm *.o -f
	rm main -f