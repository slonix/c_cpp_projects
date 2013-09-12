CC=cc
CFLAGS=-Wall

default: slicz slijent 

slicz: slicz.c err.o ports.o macs.o help_functions.o control.o
	$(CC) $(CFLAGS) -o $@ $^ -levent

err.o: err.c
	$(CC) $(CFLAGS) -c $^

ports.o: ports.c
	$(CC) $(CFLAGS) -c $^

macs.o: macs.c
	$(CC) $(CFLAGS) -c $^

help_functions.o: help_functions.c
	$(CC) $(CFLAGS) -c $^

control.o: control.c
	$(CC) $(CFLAGS) -c $^

slijent: tap-loopback.c err.o ports.o help_functions.o
	$(CC) $(CFLAGS) -o $@ $^ -levent

config:
	echo "setconfig 42421//1,2t,3t" | nc localhost 42420
	echo "setconfig 42422//2" | nc localhost 42420
	echo "setconfig 42423//3" | nc localhost 42420
	echo "getconfig" | nc localhost 42420

check:
	valgrind --leak-check=full --show-reachable=yes ./slicz -p 1234

test:
	echo 'setconfig 42401//1,15t' | nc localhost 42420

debug:
	make clean
	clear
	make
	
clean:
	rm -f *.o *~ *.swp *.swn slicz slijent

cleano:
	rm -f *.o *~
