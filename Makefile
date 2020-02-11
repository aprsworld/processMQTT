CC=gcc
CFLAGS=-I. -Wunused-function  -Wunused-variable -g

processMQTT: main.o 
	$(CC) main.o  -o processMQTT $(CFLAGS)  -lm -ljson-c -lmosquitto 


main.o: main.c
	$(CC)  -c main.c  $(CFLAGS) -I/usr/include/json-c/

