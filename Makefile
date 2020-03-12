CC=gcc
CFLAGS=-I. -Wunused-function  -Wunused-variable -g

processMQTT: processMQTT.o 
	$(CC) processMQTT.o  -o processMQTT $(CFLAGS)  -lm -ljson-c -lmosquitto 


processMQTT.o: processMQTT.c
	$(CC)  -c processMQTT.c  $(CFLAGS) -I/usr/include/json-c/

