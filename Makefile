CFLAGS+=-Wall -pedantic -g
CC= g++
all: remove_main main remove_o

main: main.o kernel.o llist.o prioll.o

remove_o:
	rm *.o -f

remove_main: 
	rm main -f
